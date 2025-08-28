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

#ifndef DATASTAX_INTERNAL_VECTOR_VALUE_HPP
#define DATASTAX_INTERNAL_VECTOR_VALUE_HPP

#include "allocated.hpp"
#include "buffer.hpp"
#include "cassandra.h"
#include "data_type.hpp"
#include "encode.hpp"
#include "external.hpp"
#include "types.hpp"

namespace datastax { namespace internal { namespace core {

class Collection;
class Tuple;
class UserTypeValue;

/**
 * VectorValue represents a vector data type for Cassandra 5.0+
 * Vectors have a fixed dimension and element type
 * Following the Tuple pattern with Allocated base and BufferVec storage
 */
class VectorValue : public Allocated {
public:
  /**
   * Create a vector with specified element type and dimension
   */
  VectorValue(CassValueType element_type, size_t dimension)
      : data_type_(new VectorType(DataType::ConstPtr(new DataType(element_type)), dimension))
      , dimension_(dimension)
      , items_() {
    items_.reserve(dimension);
  }
  
  /**
   * Create a vector from a known data type
   */
  explicit VectorValue(const DataType::ConstPtr& data_type)
      : data_type_(data_type)
      , dimension_(0)
      , items_() {
    if (data_type && data_type->value_type() == CASS_VALUE_TYPE_VECTOR) {
      const VectorType* vec_type = static_cast<const VectorType*>(data_type.get());
      dimension_ = vec_type->dimension();
      items_.reserve(dimension_);
    }
  }
  
  const DataType::ConstPtr& data_type() const { return data_type_; }
  const BufferVec& items() const { return items_; }
  size_t dimension() const { return dimension_; }
  size_t size() const { return items_.size(); }
  bool is_full() const { return items_.size() >= dimension_; }
  
  // Append functions for fixed-size types
  CassError append_bool(cass_bool_t value);
  CassError append_int8(cass_int8_t value);
  CassError append_int16(cass_int16_t value);
  CassError append_int32(cass_int32_t value);
  CassError append_int64(cass_int64_t value);
  CassError append_float(cass_float_t value);
  CassError append_double(cass_double_t value);
  CassError append_uuid(CassUuid value);
  CassError append_inet(CassInet value);
  
  // Append functions for variable-size types
  CassError append_string(CassString value);
  CassError append_bytes(CassBytes value);
  CassError append_decimal(CassDecimal value);
  CassError append_duration(CassDuration value);
  
  // Append functions for complex types
  CassError append_collection(const Collection* value);
  CassError append_tuple(const Tuple* value);
  CassError append_vector(const VectorValue* value);
  CassError append_user_type(const UserTypeValue* value);
  
  // Encoding
  Buffer encode() const;
  Buffer encode_with_length() const;
  
private:
  CassError check_append();
  CassError check_element_type(CassValueType expected_type) const;
  
  template <class T>
  CassError append_fixed(T value);
  
  template <class T>
  CassError append_variable(T value);
  
private:
  DataType::ConstPtr data_type_;
  size_t dimension_;
  BufferVec items_;
  
  DISALLOW_COPY_AND_ASSIGN(VectorValue);
};

}}} // namespace datastax::internal::core

EXTERNAL_TYPE(datastax::internal::core::VectorValue, CassVector)

#endif // DATASTAX_INTERNAL_VECTOR_VALUE_HPP