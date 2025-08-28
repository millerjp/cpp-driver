/*
  Copyright (c) DataStax, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "result_response.hpp"

#include "external.hpp"
#include "logger.hpp"
#include "protocol.hpp"
#include "result_metadata.hpp"
#include "result_response.hpp"
#include "serialization.hpp"
#include <cstdlib>

using namespace datastax;
using namespace datastax::internal;
using namespace datastax::internal::core;

extern "C" {

void cass_result_free(const CassResult* result) { result->dec_ref(); }

size_t cass_result_row_count(const CassResult* result) {
  if (result->kind() == CASS_RESULT_KIND_ROWS) {
    return result->row_count();
  }
  return 0;
}

size_t cass_result_column_count(const CassResult* result) {
  if (result->kind() == CASS_RESULT_KIND_ROWS) {
    return result->column_count();
  }
  return 0;
}

CassError cass_result_column_name(const CassResult* result, size_t index, const char** name,
                                  size_t* name_length) {
  const SharedRefPtr<ResultMetadata>& metadata(result->metadata());
  if (index >= metadata->column_count()) {
    return CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS;
  }
  if (result->kind() != CASS_RESULT_KIND_ROWS) {
    return CASS_ERROR_LIB_BAD_PARAMS;
  }
  const ColumnDefinition def = metadata->get_column_definition(index);
  *name = def.name.data();
  *name_length = def.name.size();
  return CASS_OK;
}

CassValueType cass_result_column_type(const CassResult* result, size_t index) {
  const SharedRefPtr<ResultMetadata>& metadata(result->metadata());
  if (result->kind() == CASS_RESULT_KIND_ROWS && index < metadata->column_count()) {
    return metadata->get_column_definition(index).data_type->value_type();
  }
  return CASS_VALUE_TYPE_UNKNOWN;
}

const CassDataType* cass_result_column_data_type(const CassResult* result, size_t index) {
  const SharedRefPtr<ResultMetadata>& metadata(result->metadata());
  if (result->kind() == CASS_RESULT_KIND_ROWS && index < metadata->column_count()) {
    return CassDataType::to(metadata->get_column_definition(index).data_type.get());
  }
  return NULL;
}

const CassRow* cass_result_first_row(const CassResult* result) {
  if (result->kind() == CASS_RESULT_KIND_ROWS && result->row_count() > 0) {
    return CassRow::to(&result->first_row());
  }
  return NULL;
}

cass_bool_t cass_result_has_more_pages(const CassResult* result) {
  return static_cast<cass_bool_t>(result->has_more_pages());
}

CassError cass_result_paging_state_token(const CassResult* result, const char** paging_state,
                                         size_t* paging_state_size) {
  if (!result->has_more_pages()) {
    return CASS_ERROR_LIB_NO_PAGING_STATE;
  }
  *paging_state = result->paging_state().data();
  *paging_state_size = result->paging_state().size();
  return CASS_OK;
}

} // extern "C"

class DataTypeDecoder {
public:
  DataTypeDecoder(Decoder& decoder, SimpleDataTypeCache& cache)
      : decoder_(decoder)
      , cache_(cache) {}

  DataType::ConstPtr decode() {
    decoder_.set_type("data type");
    uint16_t value_type;
    if (!decoder_.decode_uint16(value_type)) return DataType::NIL;

    switch (value_type) {
      case CASS_VALUE_TYPE_CUSTOM:
        return decode_custom();

      case CASS_VALUE_TYPE_LIST:
      case CASS_VALUE_TYPE_SET:
      case CASS_VALUE_TYPE_MAP:
        return decode_collection(static_cast<CassValueType>(value_type));

      case CASS_VALUE_TYPE_UDT:
        return decode_user_type();

      case CASS_VALUE_TYPE_TUPLE:
        return decode_tuple();

      default:
        return cache_.by_value_type(value_type);
    }
  }

private:
  DataType::ConstPtr decode_custom() {
    StringRef class_name;
    if (!decoder_.decode_string(&class_name)) return DataType::NIL;

    // Check if this is a vector type
    if (class_name.find("org.apache.cassandra.db.marshal.VectorType") != StringRef::npos) {
      // Parse the vector parameters
      // Format: "org.apache.cassandra.db.marshal.VectorType(element_type, dimension)"
      return decode_vector_from_custom(class_name);
    }

    DataType::ConstPtr type = cache_.by_class(class_name);
    if (type) return type;

    // If no mapping exists, return an actual custom type.
    return DataType::ConstPtr(new CustomType(class_name.to_string()));
  }

  DataType::ConstPtr decode_collection(CassValueType collection_type) {
    DataType::Vec types;
    types.push_back(decode());
    if (collection_type == CASS_VALUE_TYPE_MAP) {
      types.push_back(decode());
    }
    return DataType::ConstPtr(new CollectionType(collection_type, types, false));
  }

  DataType::ConstPtr decode_user_type() {
    StringRef keyspace;
    if (!decoder_.decode_string(&keyspace)) return DataType::NIL;

    StringRef type_name;
    if (!decoder_.decode_string(&type_name)) return DataType::NIL;

    uint16_t n;
    if (!decoder_.decode_uint16(n)) return DataType::NIL;

    UserType::FieldVec fields;
    for (uint16_t i = 0; i < n; ++i) {
      StringRef field_name;
      if (!decoder_.decode_string(&field_name)) return DataType::NIL;
      fields.push_back(UserType::Field(field_name.to_string(), decode()));
    }
    return DataType::ConstPtr(
        new UserType(keyspace.to_string(), type_name.to_string(), fields, false));
  }

  DataType::ConstPtr decode_tuple() {
    uint16_t n;
    if (!decoder_.decode_uint16(n)) return DataType::NIL;

    DataType::Vec types;
    for (uint16_t i = 0; i < n; ++i) {
      types.push_back(decode());
    }
    return DataType::ConstPtr(new TupleType(types, false));
  }

  DataType::ConstPtr decode_vector_from_custom(const StringRef& class_name) {
    // The class name format from Cassandra 5.0 with protocol v4 is:
    // "org.apache.cassandra.db.marshal.VectorType(element_type, dimension)"
    // For example:
    // "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.FloatType, 3)"
    
    String class_str = class_name.to_string();
    
    // Find the opening parenthesis
    size_t paren_pos = class_str.find('(');
    if (paren_pos == String::npos) {
      // No parameters - malformed vector type
      LOG_ERROR("Malformed vector custom type class name (no parameters): %s", class_str.c_str());
      return DataType::NIL;
    }
    
    // Find the closing parenthesis
    size_t close_paren = class_str.find(')', paren_pos);
    if (close_paren == String::npos) {
      // Malformed - no closing parenthesis
      LOG_ERROR("Malformed vector custom type class name (no closing parenthesis): %s", class_str.c_str());
      return DataType::NIL;
    }
    
    // Extract parameters string
    String params = class_str.substr(paren_pos + 1, close_paren - paren_pos - 1);
    
    // Find the comma separating element type and dimension
    size_t comma_pos = params.rfind(',');
    if (comma_pos == String::npos) {
      // No comma - malformed parameters
      LOG_ERROR("Malformed vector custom type parameters (no comma): %s", params.c_str());
      return DataType::NIL;
    }
    
    // Extract element type class name and dimension
    String element_class = params.substr(0, comma_pos);
    String dimension_str = params.substr(comma_pos + 1);
    
    // Trim whitespace from dimension string
    size_t first = dimension_str.find_first_not_of(' ');
    size_t last = dimension_str.find_last_not_of(' ');
    if (first != String::npos) {
      dimension_str = dimension_str.substr(first, last - first + 1);
    }
    
    // Parse dimension
    size_t dimension = 3; // Default
    char* end = NULL;
    unsigned long parsed_dim = strtoul(dimension_str.c_str(), &end, 10);
    if (end != dimension_str.c_str() && *end == '\0') {
      dimension = parsed_dim;
    }
    
    // Trim whitespace from element class
    first = element_class.find_first_not_of(' ');
    last = element_class.find_last_not_of(' ');
    if (first != String::npos) {
      element_class = element_class.substr(first, last - first + 1);
    }
    
    // Map element class name to value type
    DataType::ConstPtr element_type;
    if (element_class.find("FloatType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_FLOAT);
    } else if (element_class.find("DoubleType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_DOUBLE);
    } else if (element_class.find("Int32Type") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_INT);
    } else if (element_class.find("LongType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_BIGINT);
    } else if (element_class.find("ByteType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_TINY_INT);
    } else if (element_class.find("ShortType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_SMALL_INT);
    } else if (element_class.find("BooleanType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_BOOLEAN);
    } else if (element_class.find("UTF8Type") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_TEXT);
    } else if (element_class.find("AsciiType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_ASCII);
    } else if (element_class.find("UUIDType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_UUID);
    } else if (element_class.find("TimeUUIDType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_TIMEUUID);
    } else if (element_class.find("TimestampType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_TIMESTAMP);
    } else if (element_class.find("SimpleDateType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_DATE);
    } else if (element_class.find("TimeType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_TIME);
    } else if (element_class.find("InetAddressType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_INET);
    } else if (element_class.find("BytesType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_BLOB);
    } else if (element_class.find("DecimalType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_DECIMAL);
    } else if (element_class.find("IntegerType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_VARINT);
    } else if (element_class.find("DurationType") != String::npos) {
      element_type = cache_.by_value_type(CASS_VALUE_TYPE_DURATION);
    } else {
      // Unknown element type - log error and return NIL
      LOG_ERROR("Unknown vector element type in custom class: %s", element_class.c_str());
      return DataType::NIL;
    }
    
    return DataType::ConstPtr(new VectorType(element_type, dimension));
  }

private:
  Decoder& decoder_;
  SimpleDataTypeCache& cache_;
};

void ResultResponse::set_metadata(const ResultMetadata::Ptr& metadata) {
  metadata_ = metadata;
  decode_first_row();
}

bool ResultResponse::decode(Decoder& decoder) {
  protocol_version_ = decoder.protocol_version();
  decoder.set_type("result");
  bool is_valid = false;

  CHECK_RESULT(decoder.decode_int32(kind_));

  switch (kind_) {
    case CASS_RESULT_KIND_VOID:
      is_valid = true;
      break;

    case CASS_RESULT_KIND_ROWS:
      is_valid = decode_rows(decoder);
      break;

    case CASS_RESULT_KIND_SET_KEYSPACE:
      is_valid = decode_set_keyspace(decoder);
      break;

    case CASS_RESULT_KIND_PREPARED:
      is_valid = decode_prepared(decoder);
      break;

    case CASS_RESULT_KIND_SCHEMA_CHANGE:
      is_valid = decode_schema_change(decoder);
      break;

    default:
      assert(false);
      break;
  }

  if (!is_valid) decoder.maybe_log_remaining();
  return is_valid;
}

bool ResultResponse::decode_metadata(Decoder& decoder, ResultMetadata::Ptr* metadata,
                                     bool has_pk_indices) {
  int32_t flags = 0;
  CHECK_RESULT(decoder.decode_int32(flags));

  int32_t column_count = 0;
  CHECK_RESULT(decoder.decode_int32(column_count));

  if (flags & CASS_RESULT_FLAG_METADATA_CHANGED) {
    if (decoder.protocol_version().supports_result_metadata_id()) {
      CHECK_RESULT(decoder.decode_string(&new_metadata_id_))
    } else {
      LOG_ERROR("Metadata changed flag set with invalid protocol version %s",
                decoder.protocol_version().to_string().c_str());
      return false;
    }
  }

  if (has_pk_indices) {
    int32_t pk_count = 0;
    CHECK_RESULT(decoder.decode_int32(pk_count));
    for (int i = 0; i < pk_count; ++i) {
      uint16_t pk_index = 0;
      CHECK_RESULT(decoder.decode_uint16(pk_index));
      pk_indices_.push_back(pk_index);
    }
  }

  if (flags & CASS_RESULT_FLAG_HAS_MORE_PAGES) {
    has_more_pages_ = true;
    CHECK_RESULT(decoder.decode_bytes(&paging_state_));
  } else {
    has_more_pages_ = false;
  }

  if (!(flags & CASS_RESULT_FLAG_NO_METADATA)) {
    bool global_table_spec = flags & CASS_RESULT_FLAG_GLOBAL_TABLESPEC;

    if (global_table_spec) {
      CHECK_RESULT(decoder.decode_string(&keyspace_));
      CHECK_RESULT(decoder.decode_string(&table_));
    }

    metadata->reset(new ResultMetadata(column_count, this->buffer()));

    SimpleDataTypeCache cache;

    for (int i = 0; i < column_count; ++i) {
      ColumnDefinition def;

      def.index = i;

      if (!global_table_spec) {
        CHECK_RESULT(decoder.decode_string(&def.keyspace));
        CHECK_RESULT(decoder.decode_string(&def.table));
      }

      CHECK_RESULT(decoder.decode_string(&def.name));

      DataTypeDecoder type_decoder(decoder, cache);
      def.data_type = DataType::ConstPtr(type_decoder.decode());
      if (def.data_type == DataType::NIL) return false;

      (*metadata)->add(def);
    }
  }
  return true;
}

bool ResultResponse::decode_first_row() {
  if (row_count_ > 0 && metadata_ && // Valid metadata required for column count
      first_row_.values.empty()) {   // Only decode the first row once
    first_row_.values.reserve(column_count());
    return decode_row(row_decoder_, this, first_row_.values);
  }
  return true;
}

bool ResultResponse::decode_rows(Decoder& decoder) {
  CHECK_RESULT(decode_metadata(decoder, &metadata_));
  CHECK_RESULT(decoder.decode_int32(row_count_));
  row_decoder_ = decoder;
  CHECK_RESULT(decode_first_row());
  return true;
}

bool ResultResponse::decode_set_keyspace(Decoder& decoder) {
  CHECK_RESULT(decoder.decode_string(&keyspace_));
  return true;
}

bool ResultResponse::decode_prepared(Decoder& decoder) {
  CHECK_RESULT(decoder.decode_string(&prepared_id_));
  if (decoder.protocol_version().supports_result_metadata_id()) {
    CHECK_RESULT(decoder.decode_string(&result_metadata_id_));
  }
  CHECK_RESULT(
      decode_metadata(decoder, &metadata_, decoder.protocol_version() >= CASS_PROTOCOL_VERSION_V4));
  CHECK_RESULT(decode_metadata(decoder, &result_metadata_));
  return true;
}

bool ResultResponse::decode_schema_change(Decoder& decoder) {
  CHECK_RESULT(decoder.decode_string(&change_));
  CHECK_RESULT(decoder.decode_string(&keyspace_));
  CHECK_RESULT(decoder.decode_string(&table_));
  return true;
}
