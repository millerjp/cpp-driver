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

#ifndef DATASTAX_INTERNAL_CASS_VECTOR_HPP
#define DATASTAX_INTERNAL_CASS_VECTOR_HPP

#include "buffer.hpp"
#include "cassandra.h"
#include "encode.hpp"
#include "external.hpp"
#include "ref_counted.hpp"
#include "types.hpp"
#include "uvint.hpp"
#include "vector_type.hpp"

#define CASS_VECTOR_CHECK_TYPE(Value) \
  do {                                \
    CassError rc = check(Value);      \
    if (rc != CASS_OK) return rc;     \
  } while (0)

#define CASS_VECTOR_CHECK_DIMENSION()                                              \
  do {                                                                             \
    if (elements_.size() >= dimension_) {                                         \
      return CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS;                                  \
    }                                                                              \
  } while (0)

namespace datastax { namespace internal { namespace core {

class Collection;
class Tuple;
class UserTypeValue;

/**
 * CassandraVector represents a fixed-dimension array of elements.
 * Unlike collections, vectors have a fixed number of elements determined at type definition.
 * Elements are encoded without int32 size prefixes for fixed-length types,
 * and with UVINT prefixes for variable-length types.
 */
class CassandraVector : public RefCounted<CassandraVector> {
public:
  /**
   * Create a vector with the specified element type and dimension.
   * 
   * @param element_type The type of elements in the vector
   * @param dimension The fixed number of elements (1-8192)
   */
  CassandraVector(const DataType::ConstPtr& element_type, size_t dimension)
      : vector_type_(new VectorType(element_type, dimension))
      , dimension_(dimension) {
    elements_.reserve(dimension);
  }
  
  /**
   * Create a vector from a vector type definition.
   * 
   * @param vector_type The complete vector type definition
   */
  CassandraVector(const VectorType::ConstPtr& vector_type)
      : vector_type_(vector_type)
      , dimension_(vector_type->dimension()) {
    elements_.reserve(dimension_);
  }
  
  const VectorType::ConstPtr& data_type() const { return vector_type_; }
  const DataType::ConstPtr& element_type() const { return vector_type_->element_type(); }
  size_t dimension() const { return dimension_; }
  size_t size() const { return elements_.size(); }
  const BufferVec& elements() const { return elements_; }
  
  bool is_full() const { return elements_.size() >= dimension_; }
  
  /**
   * Append an element to the vector.
   * Returns CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS if vector is already full.
   */
#define APPEND_TYPE(Type)                     \
  CassError append(const Type value) {        \
    CASS_VECTOR_CHECK_DIMENSION();            \
    CASS_VECTOR_CHECK_TYPE(value);            \
    elements_.push_back(core::encode(value)); \
    return CASS_OK;                           \
  }
  
  APPEND_TYPE(cass_int8_t)
  APPEND_TYPE(cass_int16_t)
  APPEND_TYPE(cass_int32_t)
  APPEND_TYPE(cass_uint32_t)
  APPEND_TYPE(cass_int64_t)
  APPEND_TYPE(cass_float_t)
  APPEND_TYPE(cass_double_t)
  APPEND_TYPE(cass_bool_t)
  APPEND_TYPE(CassString)
  APPEND_TYPE(CassBytes)
  APPEND_TYPE(CassCustom)
  APPEND_TYPE(CassUuid)
  APPEND_TYPE(CassInet)
  APPEND_TYPE(CassDecimal)
  APPEND_TYPE(CassDuration)
  
#undef APPEND_TYPE
  
  CassError append(CassNull value);
  CassError append(const Collection* value);
  CassError append(const Tuple* value);
  CassError append(const UserTypeValue* value);
  CassError append(const CassandraVector* value);
  
  /**
   * Get the total encoded size of the vector elements.
   * For fixed-length types: elements are concatenated directly.
   * For variable-length types: each element is prefixed with UVINT size.
   */
  size_t get_elements_size() const;
  
  /**
   * Encode the vector elements into a buffer.
   * 
   * @param buf The buffer to write to (must have at least get_elements_size() bytes)
   */
  void encode_elements(char* buf) const;
  
  /**
   * Get the total size of the encoded vector (elements only, no length prefix).
   */
  size_t get_size() const { return get_elements_size(); }
  
  /**
   * Get the total size including a 4-byte length prefix.
   */
  size_t get_size_with_length() const { return sizeof(int32_t) + get_size(); }
  
  /**
   * Encode the vector elements into a new buffer.
   */
  Buffer encode() const;
  
  /**
   * Encode the vector with a 4-byte length prefix.
   */
  Buffer encode_with_length() const;
  
  /**
   * Clear all elements from the vector.
   */
  void clear() { elements_.clear(); }
  
private:
  template <class T>
  CassError check(const T value) {
    // Vectors must have exactly dimension_ elements
    if (elements_.size() >= dimension_) {
      return CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS;
    }
    
    // Type checking would go here - simplified for now
    IsValidDataType<T> is_valid_type;
    if (vector_type_->element_type() && !is_valid_type(value, vector_type_->element_type())) {
      return CASS_ERROR_LIB_INVALID_VALUE_TYPE;
    }
    
    return CASS_OK;
  }
  
private:
  VectorType::ConstPtr vector_type_;
  size_t dimension_;
  BufferVec elements_;
  
private:
  DISALLOW_COPY_AND_ASSIGN(CassandraVector);
};

}}} // namespace datastax::internal::core

EXTERNAL_TYPE(datastax::internal::core::CassandraVector, CassVector)

#endif // DATASTAX_INTERNAL_CASS_VECTOR_HPP