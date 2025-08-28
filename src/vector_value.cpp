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

#include "vector_value.hpp"
#include "collection.hpp"
#include "tuple.hpp"
#include "user_type_value.hpp"
#include "serialization.hpp"
#include "external.hpp"
#include "logger.hpp"
#include "vint_encoding.hpp"
#include <string.h>
#include <cassert>
#include <cstdio>

extern "C" {

CassVector* cass_vector_new(size_t dimension) {
  return CassVector::to(new datastax::internal::core::VectorValue(dimension));
}

CassVector* cass_vector_new_from_data_type(const CassDataType* data_type) {
  if (data_type == NULL) return NULL;
  
  if (data_type->value_type() != CASS_VALUE_TYPE_VECTOR) {
    return NULL;
  }
  
  return CassVector::to(new datastax::internal::core::VectorValue(
    datastax::internal::core::DataType::ConstPtr(data_type)));
}

void cass_vector_free(CassVector* vector) {
  delete vector->from();
}

const CassDataType* cass_vector_data_type(const CassVector* vector) {
  return CassDataType::to(vector->data_type().get());
}

CassError cass_vector_append_int8(CassVector* vector, cass_int8_t value) {
  return vector->append_int8(value);
}

CassError cass_vector_append_int16(CassVector* vector, cass_int16_t value) {
  return vector->append_int16(value);
}

CassError cass_vector_append_int32(CassVector* vector, cass_int32_t value) {
  return vector->append_int32(value);
}

CassError cass_vector_append_int64(CassVector* vector, cass_int64_t value) {
  return vector->append_int64(value);
}

CassError cass_vector_append_float(CassVector* vector, cass_float_t value) {
  return vector->append_float(value);
}

CassError cass_vector_append_double(CassVector* vector, cass_double_t value) {
  return vector->append_double(value);
}

CassError cass_vector_append_bool(CassVector* vector, cass_bool_t value) {
  return vector->append_bool(value);
}

CassError cass_vector_append_string(CassVector* vector, const char* value) {
  if (value == NULL) return CASS_ERROR_LIB_NULL_VALUE;
  return cass_vector_append_string_n(vector, value, strlen(value));
}

CassError cass_vector_append_string_n(CassVector* vector, const char* value, size_t value_length) {
  if (value == NULL && value_length > 0) return CASS_ERROR_LIB_NULL_VALUE;
  return vector->append_string(datastax::internal::core::CassString(value, value_length));
}

CassError cass_vector_append_bytes(CassVector* vector, const cass_byte_t* value, size_t value_size) {
  if (value == NULL && value_size > 0) return CASS_ERROR_LIB_NULL_VALUE;
  return vector->append_bytes(datastax::internal::core::CassBytes(value, value_size));
}

CassError cass_vector_append_uuid(CassVector* vector, CassUuid value) {
  return vector->append_uuid(value);
}

CassError cass_vector_append_inet(CassVector* vector, CassInet value) {
  return vector->append_inet(value);
}

CassError cass_vector_append_decimal(CassVector* vector, 
                                     const cass_byte_t* varint, size_t varint_size,
                                     cass_int32_t scale) {
  if (varint == NULL && varint_size > 0) return CASS_ERROR_LIB_NULL_VALUE;
  return vector->append_decimal(datastax::internal::core::CassDecimal(varint, varint_size, scale));
}

CassError cass_vector_append_duration(CassVector* vector,
                                      cass_int32_t months,
                                      cass_int32_t days,
                                      cass_int64_t nanos) {
  return vector->append_duration(datastax::internal::core::CassDuration(months, days, nanos));
}

CassError cass_vector_append_collection(CassVector* vector, const CassCollection* value) {
  if (value == NULL) return CASS_ERROR_LIB_NULL_VALUE;
  return vector->append_collection(value->from());
}

CassError cass_vector_append_tuple(CassVector* vector, const CassTuple* value) {
  if (value == NULL) return CASS_ERROR_LIB_NULL_VALUE;
  return vector->append_tuple(value->from());
}

CassError cass_vector_append_vector(CassVector* vector, const CassVector* value) {
  if (value == NULL) return CASS_ERROR_LIB_NULL_VALUE;
  return vector->append_vector(value->from());
}

CassError cass_vector_append_user_type(CassVector* vector, const CassUserType* value) {
  if (value == NULL) return CASS_ERROR_LIB_NULL_VALUE;
  return vector->append_user_type(value->from());
}

} // extern "C"

namespace datastax { namespace internal { namespace core {

CassError VectorValue::check_append() {
  if (is_full()) {
    return CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS;
  }
  return CASS_OK;
}

// Fixed-size type implementations
CassError VectorValue::append_bool(cass_bool_t value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_BOOLEAN)));
    }
  }
  items_.push_back(core::encode(value));
  return CASS_OK;
}

CassError VectorValue::append_int8(cass_int8_t value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TINY_INT)));
    }
  }
  items_.push_back(core::encode(value));
  return CASS_OK;
}

CassError VectorValue::append_int16(cass_int16_t value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_SMALL_INT)));
    }
  }
  items_.push_back(core::encode(value));
  return CASS_OK;
}

CassError VectorValue::append_int32(cass_int32_t value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_INT)));
    }
  }
  items_.push_back(core::encode(value));
  return CASS_OK;
}

CassError VectorValue::append_int64(cass_int64_t value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_BIGINT)));
    }
  }
  items_.push_back(core::encode(value));
  return CASS_OK;
}

CassError VectorValue::append_float(cass_float_t value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_FLOAT)));
    }
  }
  items_.push_back(core::encode(value));
  return CASS_OK;
}

CassError VectorValue::append_double(cass_double_t value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_DOUBLE)));
    }
  }
  items_.push_back(core::encode(value));
  return CASS_OK;
}

CassError VectorValue::append_uuid(CassUuid value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_UUID)));
    }
  }
  items_.push_back(core::encode(value));
  return CASS_OK;
}

CassError VectorValue::append_inet(CassInet value) {
  // INET vectors are broken in Cassandra 5.0 - cannot be deserialized properly
  LOG_ERROR("Vector elements of inet type are not properly supported by Cassandra 5.0 (deserialization fails)");
  return CASS_ERROR_LIB_INVALID_VALUE_TYPE;
}

// Variable-size type implementations
CassError VectorValue::append_string(CassString value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TEXT)));
    }
  }
  items_.push_back(core::encode_with_length(value));
  return CASS_OK;
}

CassError VectorValue::append_bytes(CassBytes value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_BLOB)));
    }
  }
  items_.push_back(core::encode_with_length(value));
  return CASS_OK;
}

CassError VectorValue::append_decimal(CassDecimal value) {
  CassError rc = check_append();
  if (rc != CASS_OK) return rc;
  // Set element type on first append
  if (items_.empty() && data_type_->value_type() == CASS_VALUE_TYPE_VECTOR) {
    VectorType* vec_type = const_cast<VectorType*>(static_cast<const VectorType*>(data_type_.get()));
    if (!vec_type->element_type()) {
      vec_type->set_element_type(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_DECIMAL)));
    }
  }
  items_.push_back(core::encode_with_length(value));
  return CASS_OK;
}

CassError VectorValue::append_duration(CassDuration value) {
  // Duration vectors are broken in Cassandra 5.0 - cannot be deserialized properly
  LOG_ERROR("Vector elements of duration type are not properly supported by Cassandra 5.0 (deserialization fails)");
  return CASS_ERROR_LIB_INVALID_VALUE_TYPE;
}

// Complex type implementations
CassError VectorValue::append_collection(const Collection* value) {
  // Collections in vectors are not properly supported by Cassandra 5.0
  LOG_ERROR("Vector elements of collection types (list, set, map) are not supported by Cassandra");
  return CASS_ERROR_LIB_INVALID_VALUE_TYPE;
}

CassError VectorValue::append_tuple(const Tuple* value) {
  // Tuples in vectors are not supported by Cassandra 5.0
  LOG_ERROR("Vector elements of tuple type are not supported by Cassandra");
  return CASS_ERROR_LIB_INVALID_VALUE_TYPE;
}

CassError VectorValue::append_vector(const VectorValue* value) {
  // Nested vectors are not supported by Cassandra 5.0
  LOG_ERROR("Nested vectors (vector of vectors) are not supported by Cassandra");
  return CASS_ERROR_LIB_INVALID_VALUE_TYPE;
}

CassError VectorValue::append_user_type(const UserTypeValue* value) {
  // User-defined types in vectors are not supported by Cassandra 5.0
  LOG_ERROR("Vector elements of user-defined type are not supported by Cassandra");
  return CASS_ERROR_LIB_INVALID_VALUE_TYPE;
}

Buffer VectorValue::encode() const {
  // Determine if element type needs length prefixes (variable-size types)
  bool is_variable_size = false;
  const VectorType* vec_type = static_cast<const VectorType*>(data_type_.get());
  DataType::ConstPtr element_type = vec_type->element_type();
  
  if (element_type) {
    switch (element_type->value_type()) {
      // Variable-size types that need length prefixes
      case CASS_VALUE_TYPE_TINY_INT:
      case CASS_VALUE_TYPE_SMALL_INT:
      case CASS_VALUE_TYPE_TEXT:
      case CASS_VALUE_TYPE_VARCHAR:
      case CASS_VALUE_TYPE_ASCII:
      case CASS_VALUE_TYPE_BLOB:
      case CASS_VALUE_TYPE_DECIMAL:
      case CASS_VALUE_TYPE_VARINT:
      case CASS_VALUE_TYPE_LIST:
      case CASS_VALUE_TYPE_SET:
      case CASS_VALUE_TYPE_MAP:
      case CASS_VALUE_TYPE_TUPLE:
      case CASS_VALUE_TYPE_UDT:
      case CASS_VALUE_TYPE_VECTOR:
        is_variable_size = true;
        break;
      // Fixed-size types that don't need length prefixes
      case CASS_VALUE_TYPE_INT:
      case CASS_VALUE_TYPE_BIGINT:
      case CASS_VALUE_TYPE_FLOAT:
      case CASS_VALUE_TYPE_DOUBLE:
      case CASS_VALUE_TYPE_BOOLEAN:
      case CASS_VALUE_TYPE_UUID:
      case CASS_VALUE_TYPE_TIMEUUID:
      case CASS_VALUE_TYPE_TIMESTAMP:
      case CASS_VALUE_TYPE_DATE:
      case CASS_VALUE_TYPE_TIME:
      case CASS_VALUE_TYPE_INET:
      case CASS_VALUE_TYPE_DURATION:
        is_variable_size = false;
        break;
      default:
        // Unknown type - log error
        LOG_ERROR("Unknown vector element type %d during encoding", element_type->value_type());
        // For safety, treat as variable-size to avoid buffer overrun
        is_variable_size = true;
        break;
    }
  }
  
  // Calculate total size including length prefixes if needed
  size_t total_size = 0;
  for (BufferVec::const_iterator i = items_.begin(), end = items_.end(); i != end; ++i) {
    if (is_variable_size) {
      // Variable-length integer prefix for variable-size types
      total_size += compute_uvint32_size(static_cast<uint32_t>(i->size())) + i->size();
    } else {
      total_size += i->size();
    }
  }
  
  // Encode items with or without length prefixes
  Buffer buf(total_size);
  size_t pos = 0;
  for (BufferVec::const_iterator i = items_.begin(), end = items_.end(); i != end; ++i) {
    if (is_variable_size) {
      // Add variable-length integer prefix for variable-size types
      pos = buf.encode_uvint32(pos, static_cast<uint32_t>(i->size()));
    }
    pos = buf.copy(pos, i->data(), i->size());
  }
  
  return buf;
}

Buffer VectorValue::encode_with_length() const {
  Buffer encoded = encode();
  
  // Add int32 length prefix
  Buffer result(sizeof(int32_t) + encoded.size());
  size_t pos = result.encode_int32(0, encoded.size());
  result.copy(pos, encoded.data(), encoded.size());
  
  return result;
}

}}} // namespace datastax::internal::core