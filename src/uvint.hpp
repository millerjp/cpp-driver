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

#ifndef DATASTAX_INTERNAL_UVINT_HPP
#define DATASTAX_INTERNAL_UVINT_HPP

#include <cstdint>
#include <cstddef>

namespace datastax { namespace internal { namespace core {

/**
 * Calculate the encoded size of an unsigned variable integer (UVINT).
 * This matches the Go driver's computeUnsignedVIntSize function.
 * 
 * @param value The value to encode
 * @return The number of bytes needed to encode the value
 */
size_t uvint_size(uint64_t value);

/**
 * Encode an unsigned variable integer (UVINT) into a buffer.
 * This matches the Go driver's writeUnsignedVInt function.
 * 
 * The encoding uses leading 1-bits to indicate the number of bytes:
 * - 0-127: 0xxxxxxx (1 byte)
 * - 128-16383: 10xxxxxx xxxxxxxx (2 bytes)
 * - 16384-2097151: 110xxxxx xxxxxxxx xxxxxxxx (3 bytes)
 * - etc.
 * 
 * @param value The value to encode
 * @param buffer The buffer to write to (must have at least uvint_size(value) bytes)
 * @return The number of bytes written
 */
size_t encode_uvint(uint64_t value, uint8_t* buffer);

/**
 * Decode an unsigned variable integer (UVINT) from a buffer.
 * This matches the Go driver's readUnsignedVInt function.
 * 
 * @param buffer The buffer to read from
 * @param size The size of the buffer
 * @param[out] value The decoded value
 * @return The number of bytes consumed, or 0 on error
 */
size_t decode_uvint(const uint8_t* buffer, size_t size, uint64_t* value);

}}} // namespace datastax::internal::core

#endif // DATASTAX_INTERNAL_UVINT_HPP