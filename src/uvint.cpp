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

#include "uvint.hpp"
#include <algorithm>

namespace datastax { namespace internal { namespace core {

// Helper function to count leading zeros in a 64-bit integer
static inline int leading_zeros_64(uint64_t value) {
  if (value == 0) return 64;
  
#ifdef __GNUC__
  return __builtin_clzll(value);
#elif defined(_MSC_VER)
  unsigned long index;
  return _BitScanReverse64(&index, value) ? (63 - index) : 64;
#else
  // Portable fallback
  int count = 0;
  uint64_t mask = 1ULL << 63;
  while ((value & mask) == 0 && mask != 0) {
    count++;
    mask >>= 1;
  }
  return count;
#endif
}

size_t uvint_size(uint64_t value) {
  // This matches Go's computeUnsignedVIntSize function:
  // lead0 := bits.LeadingZeros64(v)
  // return (639 - lead0*9) >> 6
  
  int lead0 = leading_zeros_64(value);
  return static_cast<size_t>((639 - lead0 * 9) >> 6);
}

size_t encode_uvint(uint64_t value, uint8_t* buffer) {
  // This matches Go's writeUnsignedVInt function
  
  size_t num_bytes = uvint_size(value);
  
  if (num_bytes <= 1) {
    // 0-127: single byte with high bit clear
    buffer[0] = static_cast<uint8_t>(value);
    return 1;
  }
  
  // Multi-byte encoding
  size_t extra_bytes = num_bytes - 1;
  
  // Write bytes in big-endian order
  for (size_t i = num_bytes; i > 0; i--) {
    buffer[i - 1] = static_cast<uint8_t>(value & 0xFF);
    value >>= 8;
  }
  
  // Set the leading bits in the first byte to indicate the number of bytes
  // The pattern is: (extra_bytes) number of 1 bits followed by a 0 bit
  // 1 extra byte: 10xxxxxx
  // 2 extra bytes: 110xxxxx
  // 3 extra bytes: 1110xxxx
  // etc.
  buffer[0] |= static_cast<uint8_t>(~(0xFF >> extra_bytes));
  
  return num_bytes;
}

size_t decode_uvint(const uint8_t* buffer, size_t size, uint64_t* value) {
  // This matches Go's readUnsignedVInt function
  
  if (size == 0 || buffer == nullptr || value == nullptr) {
    return 0;  // Error: insufficient data
  }
  
  uint8_t first_byte = buffer[0];
  
  // Check if it's a single-byte encoding (high bit clear)
  if ((first_byte & 0x80) == 0) {
    *value = static_cast<uint64_t>(first_byte);
    return 1;
  }
  
  // Count leading 1 bits to determine total byte count
  // This matches Go's: numBytes := bits.LeadingZeros32(uint32(^firstByte)) - 24
  uint8_t inverted = ~first_byte;
  int num_bytes = 0;
  
  // Count leading zeros in the inverted byte (which gives us the number of leading 1s in original)
  if (inverted == 0) {
    num_bytes = 8;  // All bits were 1
  } else {
    uint8_t mask = 0x80;
    while ((inverted & mask) == 0) {
      num_bytes++;
      mask >>= 1;
    }
  }
  
  // Adjust for the fact we're working with 8 bits, not 32
  // In Go: bits.LeadingZeros32 returns 24 for a byte value, we need to compensate
  num_bytes = num_bytes + 1;  // Total bytes including the first byte
  
  if (static_cast<size_t>(num_bytes) > size) {
    return 0;  // Error: insufficient data for the indicated byte count
  }
  
  // Extract the value bits from the first byte
  // Clear the leading 1 bits that indicate byte count
  uint64_t result = static_cast<uint64_t>(first_byte & (0xFF >> (num_bytes - 1)));
  
  // Read the remaining bytes
  for (int i = 1; i < num_bytes; i++) {
    result = (result << 8) | static_cast<uint64_t>(buffer[i]);
  }
  
  *value = result;
  return static_cast<size_t>(num_bytes);
}

}}} // namespace datastax::internal::core