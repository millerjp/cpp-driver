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

#ifndef DATASTAX_INTERNAL_VINT_ENCODING_HPP
#define DATASTAX_INTERNAL_VINT_ENCODING_HPP

#include "buffer.hpp"
#include <cstdint>
#include <cstddef>

namespace datastax { namespace internal { namespace core {

// Variable-length integer encoding utilities (compatible with Python uvint and Java VIntCoding)
// Uses continuation bit in MSB: 1 = more bytes follow, 0 = final byte
// Each byte stores 7 bits of actual data

inline size_t encode_uvint32(uint8_t* buf, uint32_t value) {
  size_t bytes_written = 0;
  
  // Special case for 0
  if (value == 0) {
    buf[0] = 0;
    return 1;
  }
  
  while (value > 0) {
    uint8_t byte = value & 0x7F;  // Take lowest 7 bits
    value >>= 7;
    
    if (value > 0) {
      byte |= 0x80;  // Set continuation bit if more bytes follow
    }
    
    buf[bytes_written++] = byte;
  }
  
  return bytes_written;
}

inline size_t compute_uvint32_size(uint32_t value) {
  if (value == 0) return 1;
  
  size_t size = 0;
  while (value > 0) {
    size++;
    value >>= 7;
  }
  return size;
}

inline uint32_t decode_uvint32(const uint8_t* buf, size_t* bytes_read) {
  uint32_t value = 0;
  size_t shift = 0;
  size_t index = 0;
  
  while (true) {
    uint8_t byte = buf[index++];
    value |= (static_cast<uint32_t>(byte & 0x7F) << shift);
    
    if ((byte & 0x80) == 0) {
      // No continuation bit, this is the last byte
      break;
    }
    
    shift += 7;
    if (shift >= 32) {
      // Overflow protection
      break;
    }
  }
  
  if (bytes_read) {
    *bytes_read = index;
  }
  
  return value;
}

// Bounds-safe version that checks buffer limits
inline uint32_t decode_uvint32_safe(const uint8_t* buf, size_t max_bytes, size_t* bytes_read) {
  uint32_t value = 0;
  size_t shift = 0;
  size_t index = 0;
  
  // Limit to 5 bytes max for uint32 (5*7=35 bits, enough for 32-bit value)
  size_t limit = max_bytes < 5 ? max_bytes : 5;
  
  while (index < limit) {
    uint8_t byte = buf[index++];
    value |= (static_cast<uint32_t>(byte & 0x7F) << shift);
    
    if ((byte & 0x80) == 0) {
      // No continuation bit, this is the last byte
      if (bytes_read) {
        *bytes_read = index;
      }
      return value;
    }
    
    shift += 7;
    if (shift >= 32) {
      // Would overflow uint32
      break;
    }
  }
  
  // Failed to decode complete value
  if (bytes_read) {
    *bytes_read = 0;  // Signal failure
  }
  
  return 0;
}

}}} // namespace datastax::internal::core

#endif