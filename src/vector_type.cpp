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

#include "vector_type.hpp"
#include "string.hpp"
#include "logger.hpp"
#include "data_type_parser.hpp"
#include "collection.hpp"
#include "macros.hpp"
#include <cstdlib>
#include <stdio.h>

namespace datastax { namespace internal { namespace core {

const char* VectorType::VECTOR_CLASS_NAME = "org.apache.cassandra.db.marshal.VectorType";

VectorType::ConstPtr VectorType::from_class_name(const String& class_name) {
  // Check if it starts with the vector class name
  if (class_name.find(VECTOR_CLASS_NAME) != 0) {
    // Not a vector type - this is expected for non-vector types
    return VectorType::ConstPtr();
  }
  
  // Find the opening and closing parentheses
  size_t paren_start = class_name.find('(');
  size_t paren_end = class_name.rfind(')');
  
  if (paren_start == String::npos || paren_end == String::npos || paren_start >= paren_end) {
    return VectorType::ConstPtr();
  }
  
  // Extract the parameters
  String params = class_name.substr(paren_start + 1, paren_end - paren_start - 1);
  
  // Find the comma separating element type and dimension
  // Need to handle nested types with parentheses and angle brackets
  int paren_depth = 0;
  int angle_depth = 0;
  size_t comma_pos = String::npos;
  
  for (size_t i = params.length(); i > 0; --i) {
    char c = params[i - 1];
    if (c == ')') {
      paren_depth++;
    } else if (c == '(') {
      paren_depth--;
    } else if (c == '>') {
      angle_depth++;
    } else if (c == '<') {
      angle_depth--;
    } else if (c == ',' && paren_depth == 0 && angle_depth == 0) {
      comma_pos = i - 1;
      break;
    }
  }
  
  if (comma_pos == String::npos) {
    LOG_ERROR("Failed to find dimension separator in vector params: '%s'", params.c_str());
    return VectorType::ConstPtr();
  }
  
  // Parse element type and dimension
  String element_type_str = params.substr(0, comma_pos);
  String dimension_str = params.substr(comma_pos + 1);
  
  // Trim whitespace from both strings
  size_t first = element_type_str.find_first_not_of(" \t");
  size_t last = element_type_str.find_last_not_of(" \t");
  if (first != String::npos) {
    element_type_str = element_type_str.substr(first, last - first + 1);
  }
  
  first = dimension_str.find_first_not_of(" \t");
  last = dimension_str.find_last_not_of(" \t");
  if (first != String::npos) {
    dimension_str = dimension_str.substr(first, last - first + 1);
  }
  
  // Parse dimension as integer
  char* end_ptr;
  long dimension = std::strtol(dimension_str.c_str(), &end_ptr, 10);
  
  if (*end_ptr != '\0' || dimension <= 0 || dimension > 8192) {
    LOG_ERROR("Invalid vector dimension: '%s'", dimension_str.c_str());
    return VectorType::ConstPtr();
  }
  
  // Parse element type using the existing recursive parser
  SimpleDataTypeCache cache;
  DataType::ConstPtr element_type = DataTypeClassNameParser::parse_one(element_type_str, cache);
  
  if (!element_type) {
    LOG_ERROR("Failed to parse vector element type: '%s'", element_type_str.c_str());
    // Fall back to creating a custom type with "unknown"
    element_type = DataType::ConstPtr(new CustomType("unknown"));
  }
  
  VectorType* vector = new VectorType(element_type, static_cast<size_t>(dimension));
  // Preserve the original class name we received from the server
  vector->set_class_name(class_name);
  return VectorType::ConstPtr(vector);
}

bool VectorType::is_fixed_length_element() const {
  if (!element_type_) {
    return false;
  }
  
  CassValueType type = element_type_->value_type();
  
  // Check for fixed-length types (matching Go driver's isVectorVariableLengthType)
  switch (type) {
    case CASS_VALUE_TYPE_BIGINT:
    case CASS_VALUE_TYPE_BOOLEAN:
    case CASS_VALUE_TYPE_TIMESTAMP:
    case CASS_VALUE_TYPE_DOUBLE:
    case CASS_VALUE_TYPE_FLOAT:
    case CASS_VALUE_TYPE_INT:
    case CASS_VALUE_TYPE_TIMEUUID:
    case CASS_VALUE_TYPE_UUID:
      return true;
      
    case CASS_VALUE_TYPE_CUSTOM:
      // Vectors of vectors - check the inner element type recursively
      if (element_type_->is_custom()) {
        const CustomType* custom = static_cast<const CustomType*>(element_type_.get());
        if (custom && custom->class_name().find(VECTOR_CLASS_NAME) == 0) {
          // This is a nested vector, parse it and check its element type
          VectorType::ConstPtr nested = from_class_name(custom->class_name());
          if (nested) {
            return nested->is_fixed_length_element();
          }
        }
      }
      return false;
      
    default:
      return false;
  }
}

bool VectorType::equals(const DataType::ConstPtr& data_type) const {
  if (!data_type || data_type->value_type() != CASS_VALUE_TYPE_CUSTOM) {
    return false;
  }
  
  const CustomType* custom = static_cast<const CustomType*>(data_type.get());
  if (!custom) {
    return false;
  }
  
  // Compare class names
  return class_name() == custom->class_name();
}

DataType::Ptr VectorType::copy() const {
  return DataType::Ptr(new VectorType(element_type_, dimension_));
}

String VectorType::to_string() const {
  OStringStream ss;
  ss << "vector<";
  if (element_type_) {
    ss << element_type_->to_string();
  } else {
    ss << "unknown";
  }
  ss << ", " << dimension_ << ">";
  return ss.str();
}

// Static function to get Java class name for any data type
String VectorType::get_java_class_name(const DataType::ConstPtr& type) {
  if (!type) {
    LOG_ERROR("Cannot get Java class name for null data type");
    return "";  // Return empty string to indicate error
  }
  
  switch (type->value_type()) {
    // ALL simple types from CASS_VALUE_TYPE_MAPPING
    case CASS_VALUE_TYPE_ASCII:
      return "org.apache.cassandra.db.marshal.AsciiType";
    case CASS_VALUE_TYPE_BIGINT:
      return "org.apache.cassandra.db.marshal.LongType";
    case CASS_VALUE_TYPE_BLOB:
      return "org.apache.cassandra.db.marshal.BytesType";
    case CASS_VALUE_TYPE_BOOLEAN:
      return "org.apache.cassandra.db.marshal.BooleanType";
    case CASS_VALUE_TYPE_COUNTER:
      return "org.apache.cassandra.db.marshal.CounterColumnType";
    case CASS_VALUE_TYPE_DECIMAL:
      return "org.apache.cassandra.db.marshal.DecimalType";
    case CASS_VALUE_TYPE_DOUBLE:
      return "org.apache.cassandra.db.marshal.DoubleType";
    case CASS_VALUE_TYPE_FLOAT:
      return "org.apache.cassandra.db.marshal.FloatType";
    case CASS_VALUE_TYPE_INT:
      return "org.apache.cassandra.db.marshal.Int32Type";
    case CASS_VALUE_TYPE_TEXT:
    case CASS_VALUE_TYPE_VARCHAR:
      return "org.apache.cassandra.db.marshal.UTF8Type";
    case CASS_VALUE_TYPE_TIMESTAMP:
      return "org.apache.cassandra.db.marshal.TimestampType";
    case CASS_VALUE_TYPE_UUID:
      return "org.apache.cassandra.db.marshal.UUIDType";
    case CASS_VALUE_TYPE_VARINT:
      return "org.apache.cassandra.db.marshal.IntegerType";
    case CASS_VALUE_TYPE_TIMEUUID:
      return "org.apache.cassandra.db.marshal.TimeUUIDType";
    case CASS_VALUE_TYPE_INET:
      return "org.apache.cassandra.db.marshal.InetAddressType";
    case CASS_VALUE_TYPE_DATE:
      return "org.apache.cassandra.db.marshal.SimpleDateType";
    case CASS_VALUE_TYPE_TIME:
      return "org.apache.cassandra.db.marshal.TimeType";
    case CASS_VALUE_TYPE_SMALL_INT:
      return "org.apache.cassandra.db.marshal.ShortType";
    case CASS_VALUE_TYPE_TINY_INT:
      return "org.apache.cassandra.db.marshal.ByteType";
    case CASS_VALUE_TYPE_DURATION:
      return "org.apache.cassandra.db.marshal.DurationType";
      
    // Collection types - recursively handle element types
    case CASS_VALUE_TYPE_LIST:
      {
        const CollectionType* collection = static_cast<const CollectionType*>(type.get());
        if (!collection) {
          LOG_ERROR("LIST type is not a CollectionType");
          return "";
        }
        if (collection->types().empty()) {
          LOG_ERROR("LIST type has no element type");
          return "";
        }
        String elem_class = get_java_class_name(collection->types()[0]);
        if (elem_class.empty()) {
          LOG_ERROR("Failed to get Java class name for LIST element type");
          return "";
        }
        OStringStream ss;
        ss << "org.apache.cassandra.db.marshal.ListType(" << elem_class << ")";
        return ss.str();
      }
      
    case CASS_VALUE_TYPE_SET:
      {
        const CollectionType* collection = static_cast<const CollectionType*>(type.get());
        if (!collection) {
          LOG_ERROR("SET type is not a CollectionType");
          return "";
        }
        if (collection->types().empty()) {
          LOG_ERROR("SET type has no element type");
          return "";
        }
        String elem_class = get_java_class_name(collection->types()[0]);
        if (elem_class.empty()) {
          LOG_ERROR("Failed to get Java class name for SET element type");
          return "";
        }
        OStringStream ss;
        ss << "org.apache.cassandra.db.marshal.SetType(" << elem_class << ")";
        return ss.str();
      }
      
    case CASS_VALUE_TYPE_MAP:
      {
        const CollectionType* collection = static_cast<const CollectionType*>(type.get());
        if (!collection) {
          LOG_ERROR("MAP type is not a CollectionType");
          return "";
        }
        if (collection->types().size() < 2) {
          LOG_ERROR("MAP type has insufficient types (need key and value)");
          return "";
        }
        String key_class = get_java_class_name(collection->types()[0]);
        if (key_class.empty()) {
          LOG_ERROR("Failed to get Java class name for MAP key type");
          return "";
        }
        String val_class = get_java_class_name(collection->types()[1]);
        if (val_class.empty()) {
          LOG_ERROR("Failed to get Java class name for MAP value type");
          return "";
        }
        OStringStream ss;
        ss << "org.apache.cassandra.db.marshal.MapType(" << key_class << "," << val_class << ")";
        return ss.str();
      }
      
    case CASS_VALUE_TYPE_TUPLE:
      {
        const TupleType* tuple = static_cast<const TupleType*>(type.get());
        if (!tuple) {
          LOG_ERROR("TUPLE type is not a TupleType");
          return "";
        }
        OStringStream ss;
        ss << "org.apache.cassandra.db.marshal.TupleType(";
        bool first = true;
        for (size_t i = 0; i < tuple->types().size(); ++i) {
          if (!first) ss << ",";
          String elem_class = get_java_class_name(tuple->types()[i]);
          if (elem_class.empty()) {
            LOG_ERROR("Failed to get Java class name for TUPLE element %zu", i);
            return "";
          }
          ss << elem_class;
          first = false;
        }
        ss << ")";
        return ss.str();
      }
      
    case CASS_VALUE_TYPE_UDT:
      {
        const UserType* udt = static_cast<const UserType*>(type.get());
        if (!udt) {
          LOG_ERROR("UDT type is not a UserType");
          return "";
        }
        OStringStream ss;
        ss << "org.apache.cassandra.db.marshal.UserType(";
        ss << udt->keyspace() << ",";
        // Convert UDT name to hex
        const String& name = udt->type_name();
        for (size_t i = 0; i < name.size(); ++i) {
          char buf[3];
          snprintf(buf, sizeof(buf), "%02x", (unsigned char)name[i]);
          ss << buf;
        }
        // Add field definitions
        for (size_t i = 0; i < udt->fields().size(); ++i) {
          const UserType::Field& field = udt->fields()[i];
          ss << ",";
          // Field name in hex
          for (size_t j = 0; j < field.name.size(); ++j) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02x", (unsigned char)field.name[j]);
            ss << buf;
          }
          ss << ":";
          String field_class = get_java_class_name(field.type);
          if (field_class.empty()) {
            LOG_ERROR("Failed to get Java class name for UDT field '%s'", field.name.c_str());
            return "";
          }
          ss << field_class;
        }
        ss << ")";
        return ss.str();
      }
      
    case CASS_VALUE_TYPE_CUSTOM:
      {
        const CustomType* custom = static_cast<const CustomType*>(type.get());
        if (!custom) {
          LOG_ERROR("CUSTOM type is not a CustomType");
          return "";
        }
        // Custom types already have their full Java class name
        return custom->class_name();
      }
      
    default:
      LOG_ERROR("Unknown or unsupported value type %d for Java class name conversion", type->value_type());
      return "";
  }
}

void VectorType::update_class_name() {
  if (!element_type_) {
    LOG_ERROR("Cannot update class name for vector with null element type");
    set_class_name(VECTOR_CLASS_NAME);  // Set base name as fallback
    return;
  }
  
  String element_class = get_java_class_name(element_type_);
  if (element_class.empty()) {
    LOG_ERROR("Failed to get Java class name for vector element type %d - vector will not work properly", 
              element_type_->value_type());
    // Still need to set something, but this indicates an error
    OStringStream ss;
    ss << VECTOR_CLASS_NAME << "(ERROR , " << dimension_ << ")";
    set_class_name(ss.str());
    return;
  }
  
  OStringStream ss;
  ss << VECTOR_CLASS_NAME << "(";
  ss << element_class;
  ss << " , " << dimension_ << ")";  // Space before comma to match Cassandra format
  set_class_name(ss.str());
}

}}} // namespace datastax::internal::core