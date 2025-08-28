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

#include "buffer.hpp"
#include "vint_encoding.hpp"

namespace datastax { namespace internal { namespace core {

size_t Buffer::encode_uvint32(size_t offset, uint32_t value) {
  size_t size = compute_uvint32_size(value);
  assert(offset + size <= static_cast<size_t>(size_));
  
  size_t bytes_written = core::encode_uvint32(reinterpret_cast<uint8_t*>(data() + offset), value);
  return offset + bytes_written;
}

}}} // namespace datastax::internal::core