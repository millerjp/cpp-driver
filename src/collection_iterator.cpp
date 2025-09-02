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
    , index_(-1) {
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
      // Fallback: treat as empty vector
      dimension_ = 0;
      is_fixed_length_ = true;
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
  
  // Vectors encoded differently than collections:
  // - Fixed-length types: no size prefix, just the value
  // - Variable-length types: UVINT size prefix + value
  
  // For now, let's just decode as if it were a normal value
  // The decoder should handle the appropriate format
  value_ = decoder_.decode_value(element_type_);
  
  return value_.is_valid();
}
