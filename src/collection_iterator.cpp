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

#include "collection_iterator.hpp"
#include "vector_type.hpp"
#include "uvint.hpp"
#include "logger.hpp"

using namespace datastax::internal::core;

bool CollectionIterator::next() {
  if (index_ + 1 >= count_) {
    return false;
  }
  ++index_;
  return decode_value();
}

bool CollectionIterator::decode_value() {
  if (collection_->value_type() == CASS_VALUE_TYPE_MAP) {
    const DataType::ConstPtr& data_type =
        (index_ % 2 == 0) ? collection_->primary_data_type() : collection_->secondary_data_type();
    value_ = decoder_.decode_value(data_type);
  } else {
    value_ = decoder_.decode_value(collection_->primary_data_type());
  }

  return value_.is_valid();
}

bool TupleIterator::next() {
  if (next_ == end_) {
    return false;
  }
  current_ = next_++;

  value_ = decoder_.decode_value(*current_);
  return value_.is_valid();
}

VectorIterator::VectorIterator(const Value* vector)
    : ValueIterator(CASS_ITERATOR_TYPE_VECTOR, vector->decoder())
    , vector_(vector)
    , is_fixed_length_(true)
    , index_(-1)
    , dimension_(0) {
  // Get the vector type to extract element type and dimension
  const DataType* data_type = vector_->data_type().get();
  if (data_type && data_type->value_type() == CASS_VALUE_TYPE_CUSTOM) {
    const CustomType* custom_type = static_cast<const CustomType*>(data_type);
    // Parse the vector type from the custom class name
    VectorType::ConstPtr vector_type = VectorType::from_class_name(custom_type->class_name());
    if (vector_type) {
      element_type_ = vector_type->element_type();
      dimension_ = vector_type->dimension();
      is_fixed_length_ = vector_type->is_fixed_length_element();
    } else {
      // Failed to parse - for now, hardcode for testing
      // Try to guess from the class name
      String class_name = custom_type->class_name();
      
      // Check if it contains VectorType at all
      if (class_name.find("VectorType") != String::npos) {
        // Temporary hardcoding - parse basic types from class name
        // TODO: Properly parse the class name format from server
        if (class_name.find("FloatType") != String::npos) {
          element_type_ = DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_FLOAT));
          is_fixed_length_ = true;
          
          // Try to extract dimension from class name
          // Format: "...VectorType(org.apache.cassandra.db.marshal.FloatType, N)"
          size_t comma_pos = class_name.rfind(',');
          size_t paren_pos = class_name.rfind(')');
          if (comma_pos != String::npos && paren_pos != String::npos && comma_pos < paren_pos) {
            String dim_str = class_name.substr(comma_pos + 1, paren_pos - comma_pos - 1);
            // Trim whitespace
            size_t start = dim_str.find_first_not_of(" \t");
            if (start != String::npos) {
              dimension_ = atoi(dim_str.substr(start).c_str());
              if (dimension_ <= 0 || dimension_ > 8192) {
                // Invalid dimension - fail
                LOG_ERROR("Invalid vector dimension parsed: %d from '%s'", 
                         dimension_, class_name.c_str());
                element_type_.reset();
                dimension_ = 0;
              }
            } else {
              // Failed to parse dimension
              LOG_ERROR("Failed to parse vector dimension from: '%s'", class_name.c_str());
              element_type_.reset();
              dimension_ = 0;
            }
          } else {
            // Failed to find dimension in class name
            LOG_ERROR("Failed to find vector dimension in class name: '%s'", class_name.c_str());
            element_type_.reset();
            dimension_ = 0;
          }
        } else {
          // Unknown vector element type - fail
          LOG_ERROR("Unknown vector element type in class name: '%s'", class_name.c_str());
          element_type_.reset();
          dimension_ = 0;
          is_fixed_length_ = true;
        }
      } else {
        // Fallback: treat as empty vector
        dimension_ = 0;
        is_fixed_length_ = true;
      }
    }
  } else {
    // Not a vector type, shouldn't happen but handle gracefully
    dimension_ = 0;
    is_fixed_length_ = true;
  }
}

bool VectorIterator::next() {
  if (index_ + 1 >= dimension_) {
    return false;
  }
  ++index_;
  return decode_value();
}

bool VectorIterator::decode_value() {
  if (!element_type_) {
    return false;
  }
  
  // Vectors encode elements differently than collections:
  // - Fixed-length types: no size prefix, just raw bytes
  // - Variable-length types: UVINT size prefix (not int32)
  // 
  // The standard decode_value() expects int32 size prefix, so we can't use it directly.
  // Instead, we use our specialized decode_vector_element() method.
  value_ = decoder_.decode_vector_element(element_type_, is_fixed_length_);
  
  
  // Check if the value was decoded successfully
  if (!value_.is_valid()) {
    return false;
  }
  
  return true;
}
