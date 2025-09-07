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

#include "cass_vector.hpp"
#include "collection.hpp"
#include "constants.hpp"
#include "external.hpp"
#include "logger.hpp"
#include "macros.hpp"
#include "tuple.hpp"
#include "user_type_value.hpp"

#include <string.h>

using namespace datastax;
using namespace datastax::internal::core;

extern "C" {

CassVector* cass_vector_new(CassValueType element_type, size_t dimension) {
  if (dimension == 0 || dimension > 8192) {
    LOG_ERROR("Cannot create vector: dimension %zu is out of range (1-8192)", dimension);
    return NULL;
  }
  
  // Reject types that require subtypes - these cannot be properly constructed
  // with just an enum value as they need inner type information
  switch (element_type) {
    case CASS_VALUE_TYPE_LIST:
    case CASS_VALUE_TYPE_SET:
    case CASS_VALUE_TYPE_MAP:
    case CASS_VALUE_TYPE_TUPLE:
    case CASS_VALUE_TYPE_UDT:
    case CASS_VALUE_TYPE_CUSTOM:
      // These types require subtypes and cannot be created with this API
      // Use cass_vector_new_from_data_type() with a properly constructed DataType instead
      LOG_ERROR("Cannot create vector with element type %d using cass_vector_new(). "
                "Types that require subtypes (LIST, SET, MAP, TUPLE, UDT, CUSTOM) "
                "must use cass_vector_new_from_data_type() with a properly constructed DataType.",
                static_cast<int>(element_type));
      return NULL;
    default:
      // Primitive types are OK
      break;
  }
  
  DataType::ConstPtr type(new DataType(element_type));
  CassandraVector* vector = new CassandraVector(type, dimension);
  vector->inc_ref();
  return CassVector::to(vector);
}

CassVector* cass_vector_new_from_data_type(const CassDataType* data_type) {
  if (!data_type || !data_type->from()->is_custom()) {
    return NULL;
  }
  
  const CustomType* custom = static_cast<const CustomType*>(data_type->from());
  VectorType::ConstPtr vector_type = VectorType::from_class_name(custom->class_name());
  if (!vector_type) {
    return NULL;
  }
  
  CassandraVector* vector = new CassandraVector(vector_type);
  vector->inc_ref();
  return CassVector::to(vector);
}

CassVector* cass_vector_new_with_element_type(const CassDataType* element_data_type,
                                              size_t dimension) {
  if (!element_data_type) {
    LOG_ERROR("Cannot create vector: element_data_type is NULL");
    return NULL;
  }
  
  if (dimension == 0 || dimension > 8192) {
    LOG_ERROR("Cannot create vector: dimension %zu is out of range (1-8192)", dimension);
    return NULL;
  }
  
  // Create a DataType pointer from the C API type
  DataType::ConstPtr element_type(element_data_type->from());
  
  // Create the vector with the specified element type and dimension
  CassandraVector* vector = new CassandraVector(element_type, dimension);
  vector->inc_ref();
  return CassVector::to(vector);
}

void cass_vector_free(CassVector* vector) { 
  vector->dec_ref(); 
}

const CassDataType* cass_vector_data_type(const CassVector* vector) {
  return CassDataType::to(vector->data_type().get());
}

const CassDataType* cass_vector_element_data_type(const CassVector* vector) {
  return CassDataType::to(vector->element_type().get());
}

size_t cass_vector_dimension(const CassVector* vector) {
  return vector->dimension();
}

#define CASS_VECTOR_APPEND(Name, Params, Value)                         \
  CassError cass_vector_append_##Name(CassVector* vector Params) {      \
    return vector->append(Value);                                       \
  }

CASS_VECTOR_APPEND(null, ZERO_PARAMS_(), CassNull())
CASS_VECTOR_APPEND(int8, ONE_PARAM_(cass_int8_t value), value)
CASS_VECTOR_APPEND(int16, ONE_PARAM_(cass_int16_t value), value)
CASS_VECTOR_APPEND(int32, ONE_PARAM_(cass_int32_t value), value)
CASS_VECTOR_APPEND(uint32, ONE_PARAM_(cass_uint32_t value), value)
CASS_VECTOR_APPEND(int64, ONE_PARAM_(cass_int64_t value), value)
CASS_VECTOR_APPEND(float, ONE_PARAM_(cass_float_t value), value)
CASS_VECTOR_APPEND(double, ONE_PARAM_(cass_double_t value), value)
CASS_VECTOR_APPEND(bool, ONE_PARAM_(cass_bool_t value), value)
CASS_VECTOR_APPEND(uuid, ONE_PARAM_(CassUuid value), value)
CASS_VECTOR_APPEND(inet, ONE_PARAM_(CassInet value), value)
CASS_VECTOR_APPEND(collection, ONE_PARAM_(const CassCollection* value), value)
CASS_VECTOR_APPEND(tuple, ONE_PARAM_(const CassTuple* value), value)
CASS_VECTOR_APPEND(user_type, ONE_PARAM_(const CassUserType* value), value)
CASS_VECTOR_APPEND(vector, ONE_PARAM_(const CassVector* value), value)
CASS_VECTOR_APPEND(bytes, TWO_PARAMS_(const cass_byte_t* value, size_t value_size),
                   CassBytes(value, value_size))
CASS_VECTOR_APPEND(decimal,
                   THREE_PARAMS_(const cass_byte_t* varint, size_t varint_size, int scale),
                   CassDecimal(varint, varint_size, scale))
CASS_VECTOR_APPEND(duration,
                   THREE_PARAMS_(cass_int32_t months, cass_int32_t days, cass_int64_t nanos),
                   CassDuration(months, days, nanos))

#undef CASS_VECTOR_APPEND

CassError cass_vector_append_string(CassVector* vector, const char* value) {
  return vector->append(CassString(value, SAFE_STRLEN(value)));
}

CassError cass_vector_append_string_n(CassVector* vector, const char* value, size_t value_length) {
  return vector->append(CassString(value, value_length));
}

CassError cass_vector_append_custom(CassVector* vector, const char* class_name,
                                    const cass_byte_t* value, size_t value_size) {
  return vector->append(CassCustom(StringRef(class_name), value, value_size));
}

CassError cass_vector_append_custom_n(CassVector* vector, const char* class_name,
                                      size_t class_name_length, const cass_byte_t* value,
                                      size_t value_size) {
  return vector->append(CassCustom(StringRef(class_name, class_name_length), value, value_size));
}

CassError cass_vector_append_date(CassVector* vector, cass_uint32_t value) {
  return vector->append(CassDate(value));
}

CassError cass_vector_append_time(CassVector* vector, cass_int64_t value) {
  return vector->append(CassTime(value));
}

CassError cass_vector_append_timestamp(CassVector* vector, cass_int64_t value) {
  return vector->append(CassTimestamp(value));
}

CassError cass_vector_append_varint(CassVector* vector, const cass_byte_t* varint,
                                    size_t varint_size) {
  return vector->append(CassVarint(varint, varint_size));
}

} // extern "C"

namespace datastax { namespace internal { namespace core {

CassError CassandraVector::append(CassNull value) {
  // Vectors don't support null elements
  return CASS_ERROR_LIB_NULL_VALUE;
}

CassError CassandraVector::append(const Collection* value) {
  CASS_VECTOR_CHECK_DIMENSION();
  CASS_VECTOR_CHECK_TYPE(value);
  elements_.push_back(value->encode());
  return CASS_OK;
}

CassError CassandraVector::append(const Tuple* value) {
  CASS_VECTOR_CHECK_DIMENSION();
  CASS_VECTOR_CHECK_TYPE(value);
  elements_.push_back(value->encode());
  return CASS_OK;
}

CassError CassandraVector::append(const UserTypeValue* value) {
  CASS_VECTOR_CHECK_DIMENSION();
  CASS_VECTOR_CHECK_TYPE(value);
  elements_.push_back(value->encode());
  return CASS_OK;
}

CassError CassandraVector::append(const CassandraVector* value) {
  CASS_VECTOR_CHECK_DIMENSION();
  CASS_VECTOR_CHECK_TYPE(value);
  elements_.push_back(value->encode());
  return CASS_OK;
}

size_t CassandraVector::get_elements_size() const {
  size_t total_size = 0;
  bool is_fixed_length = vector_type_->is_fixed_length_element();
  
  for (size_t i = 0; i < elements_.size(); ++i) {
    const Buffer& element = elements_[i];
    if (!is_fixed_length) {
      // Variable-length elements need UVINT size prefix
      total_size += uvint_size(element.size());
    }
    total_size += element.size();
  }
  
  return total_size;
}

void CassandraVector::encode_elements(char* buf) const {
  bool is_fixed_length = vector_type_->is_fixed_length_element();
  size_t offset = 0;
  
  for (size_t i = 0; i < elements_.size(); ++i) {
    const Buffer& element = elements_[i];
    
    if (!is_fixed_length) {
      // Encode UVINT size prefix for variable-length elements
      uint8_t* uvint_buf = reinterpret_cast<uint8_t*>(buf + offset);
      size_t uvint_bytes = encode_uvint(element.size(), uvint_buf);
      offset += uvint_bytes;
    }
    
    // Copy element data
    memcpy(buf + offset, element.data(), element.size());
    offset += element.size();
  }
}

Buffer CassandraVector::encode() const {
  size_t size = get_elements_size();
  Buffer result(size);
  encode_elements(result.data());
  return result;
}

Buffer CassandraVector::encode_with_length() const {
  size_t elements_size = get_elements_size();
  Buffer result(sizeof(int32_t) + elements_size);
  
  // Encode length prefix
  result.encode_int32(0, static_cast<int32_t>(elements_size));
  
  // Encode elements
  encode_elements(result.data() + sizeof(int32_t));
  
  return result;
}

}}} // namespace datastax::internal::core