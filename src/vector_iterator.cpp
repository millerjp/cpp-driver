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

#include "vector_iterator.hpp"
#include "vector_iterator_helpers.hpp"
#include "serialization.hpp"
#include "vint_encoding.hpp"
#include "logger.hpp"

using namespace datastax::internal::core;

bool VectorIterator::next() {
  if (current_index_ >= dimension_) {
    return false;
  }
  
  // Determine if element type needs length prefixes (variable-size types)
  CassValueType type = element_type_->value_type();
  bool is_variable_size = false;
  int fixed_size = 0;
  
  switch (type) {
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
    case CASS_VALUE_TYPE_BOOLEAN:
      fixed_size = 1;
      break;
    case CASS_VALUE_TYPE_INT:
    case CASS_VALUE_TYPE_FLOAT:
      fixed_size = 4;
      break;
    case CASS_VALUE_TYPE_BIGINT:
    case CASS_VALUE_TYPE_COUNTER:
    case CASS_VALUE_TYPE_DOUBLE:
      fixed_size = 8;
      break;
    case CASS_VALUE_TYPE_UUID:
    case CASS_VALUE_TYPE_TIMEUUID:
      fixed_size = 16;
      break;
    case CASS_VALUE_TYPE_DATE:
      fixed_size = 4;
      break;
    case CASS_VALUE_TYPE_TIMESTAMP:
    case CASS_VALUE_TYPE_TIME:
      fixed_size = 8;
      break;
    case CASS_VALUE_TYPE_INET:
      // INET can be 4 bytes (IPv4) or 16 bytes (IPv6)
      // It's variable size and needs a prefix
      is_variable_size = true;
      break;
    case CASS_VALUE_TYPE_DURATION:
      // Duration is variable-size type
      is_variable_size = true;
      break;
    default:
      // Unknown type - log error and fail
      LOG_ERROR("Unknown vector element type %d in vector iterator", type);
      return false;
  }
  
  if (is_variable_size) {
    // Read variable-length integer size prefix
    if (decoder_.remaining_ < 1) {
      return false;
    }
    
    size_t bytes_read = 0;
    uint32_t size = decode_uvint32(reinterpret_cast<const uint8_t*>(decoder_.input_), &bytes_read);
    
    if (decoder_.remaining_ < bytes_read + size) {
      return false;
    }
    
    decoder_.input_ += bytes_read;
    decoder_.remaining_ -= bytes_read;
    
    // Create a decoder for just this element's data
    Decoder element_decoder(decoder_.input_, size, decoder_.protocol_version_);
    decoder_.input_ += size;
    decoder_.remaining_ -= size;
    
    value_ = Value(element_type_, element_decoder);
  } else {
    // Fixed-size types: no size prefix
    if (decoder_.remaining_ < static_cast<size_t>(fixed_size)) {
      return false;
    }
    
    Decoder element_decoder(decoder_.input_, fixed_size, decoder_.protocol_version_);
    decoder_.input_ += fixed_size;
    decoder_.remaining_ -= fixed_size;
    
    value_ = Value(element_type_, element_decoder);
  }
  
  current_index_++;
  return value_.is_valid();
}