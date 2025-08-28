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

#ifndef DATASTAX_INTERNAL_VECTOR_ITERATOR_HPP
#define DATASTAX_INTERNAL_VECTOR_ITERATOR_HPP

#include "collection_iterator.hpp" // For ValueIterator base class (not CollectionIterator)
#include "data_type.hpp"
#include "value.hpp"

namespace datastax { namespace internal { namespace core {

// VectorIterator is separate from CollectionIterator but shares the ValueIterator
// base class since both iterate over values
class VectorIterator : public ValueIterator {
public:
  VectorIterator(const Value* vector)
      : ValueIterator(CASS_ITERATOR_TYPE_COLLECTION, vector->decoder()) {
    // Vectors have a single element type repeated for each dimension
    const VectorType* vector_type = static_cast<const VectorType*>(vector->data_type().get());
    element_type_ = vector_type->element_type();
    dimension_ = vector_type->dimension();
    current_index_ = 0;
  }

  virtual bool next();

private:
  DataType::ConstPtr element_type_;
  size_t dimension_;
  size_t current_index_;
};

}}} // namespace datastax::internal::core

#endif