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
#include <cstdlib>

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
  // Need to handle nested types like VectorType(ListType(Int32Type), 3)
  int paren_depth = 0;
  size_t comma_pos = String::npos;
  
  for (size_t i = 0; i < params.length(); ++i) {
    if (params[i] == '(') {
      paren_depth++;
    } else if (params[i] == ')') {
      paren_depth--;
    } else if (params[i] == ',' && paren_depth == 0) {
      comma_pos = i;
      break;
    }
  }
  
  if (comma_pos == String::npos) {
    return VectorType::ConstPtr();
  }
  
  // Parse element type and dimension
  String element_type_str = params.substr(0, comma_pos);
  String dimension_str = params.substr(comma_pos + 1);
  
  // Trim whitespace from dimension string
  size_t first = dimension_str.find_first_not_of(" \t");
  if (first != String::npos) {
    dimension_str = dimension_str.substr(first);
  }
  
  // Parse dimension as integer
  char* end_ptr;
  long dimension = std::strtol(dimension_str.c_str(), &end_ptr, 10);
  
  if (*end_ptr != '\0' || dimension <= 0 || dimension > 8192) {
    return VectorType::ConstPtr();
  }
  
  // Parse element type - handle both simple and complex types
  DataType::ConstPtr element_type;
  
  // Check for collection types first
  if (element_type_str.find("org.apache.cassandra.db.marshal.ListType") == 0) {
    // For now, we can't fully parse nested collection types without more context
    // Log the issue and fail explicitly
    LOG_ERROR("Cannot parse nested collection type in vector: '%s' - not yet implemented", element_type_str.c_str());
    return VectorType::ConstPtr();
  } else if (element_type_str.find("org.apache.cassandra.db.marshal.SetType") == 0) {
    LOG_ERROR("Cannot parse nested set type in vector: '%s' - not yet implemented", element_type_str.c_str());
    return VectorType::ConstPtr();
  } else if (element_type_str.find("org.apache.cassandra.db.marshal.MapType") == 0) {
    LOG_ERROR("Cannot parse nested map type in vector: '%s' - not yet implemented", element_type_str.c_str());
    return VectorType::ConstPtr();
  } else {
    // Try to parse as simple type
    element_type = DataType::create_by_class(element_type_str);
    if (!element_type) {
      LOG_ERROR("Failed to create DataType from class: '%s' - unknown type", element_type_str.c_str());
      return VectorType::ConstPtr();
    }
  }
  
  LOG_ERROR("[DEBUG] Successfully created element type, value_type=%d", element_type->value_type());
  
  return VectorType::ConstPtr(new VectorType(element_type, static_cast<size_t>(dimension)));
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

void VectorType::update_class_name() {
  OStringStream ss;
  ss << VECTOR_CLASS_NAME << "(";
  
  // Convert element type to Java class name format
  if (element_type_) {
    // This would need to map C++ types to Java class names
    // For now, using a simplified version
    switch (element_type_->value_type()) {
      case CASS_VALUE_TYPE_INT:
        ss << "org.apache.cassandra.db.marshal.Int32Type";
        break;
      case CASS_VALUE_TYPE_BIGINT:
        ss << "org.apache.cassandra.db.marshal.LongType";
        break;
      case CASS_VALUE_TYPE_FLOAT:
        ss << "org.apache.cassandra.db.marshal.FloatType";
        break;
      case CASS_VALUE_TYPE_DOUBLE:
        ss << "org.apache.cassandra.db.marshal.DoubleType";
        break;
      case CASS_VALUE_TYPE_TEXT:
      case CASS_VALUE_TYPE_VARCHAR:
        ss << "org.apache.cassandra.db.marshal.UTF8Type";
        break;
      case CASS_VALUE_TYPE_BOOLEAN:
        ss << "org.apache.cassandra.db.marshal.BooleanType";
        break;
      case CASS_VALUE_TYPE_UUID:
        ss << "org.apache.cassandra.db.marshal.UUIDType";
        break;
      case CASS_VALUE_TYPE_TIMEUUID:
        ss << "org.apache.cassandra.db.marshal.TimeUUIDType";
        break;
      case CASS_VALUE_TYPE_TIMESTAMP:
        ss << "org.apache.cassandra.db.marshal.TimestampType";
        break;
      case CASS_VALUE_TYPE_BLOB:
        ss << "org.apache.cassandra.db.marshal.BytesType";
        break;
      case CASS_VALUE_TYPE_CUSTOM:
        // For custom types (like nested vectors), use their class name
        {
          const CustomType* custom = static_cast<const CustomType*>(element_type_.get());
          if (custom) {
            ss << custom->class_name();
          } else {
            ss << "unknown";
          }
        }
        break;
      default:
        ss << "unknown";
        break;
    }
  } else {
    ss << "unknown";
  }
  
  ss << ", " << dimension_ << ")";
  set_class_name(ss.str());
}

}}} // namespace datastax::internal::core