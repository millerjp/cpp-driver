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

#ifndef DATASTAX_INTERNAL_VECTOR_ITERATOR_HELPERS_HPP
#define DATASTAX_INTERNAL_VECTOR_ITERATOR_HELPERS_HPP

#include "cassandra.h"

namespace datastax { namespace internal { namespace core {

inline bool is_fixed_size_type(CassValueType type) {
  switch (type) {
    case CASS_VALUE_TYPE_BOOLEAN:
    case CASS_VALUE_TYPE_TINY_INT:
    case CASS_VALUE_TYPE_SMALL_INT:
    case CASS_VALUE_TYPE_INT:
    case CASS_VALUE_TYPE_DATE:
    case CASS_VALUE_TYPE_BIGINT:
    case CASS_VALUE_TYPE_COUNTER:
    case CASS_VALUE_TYPE_TIMESTAMP:
    case CASS_VALUE_TYPE_TIME:
    case CASS_VALUE_TYPE_FLOAT:
    case CASS_VALUE_TYPE_DOUBLE:
    case CASS_VALUE_TYPE_UUID:
    case CASS_VALUE_TYPE_TIMEUUID:
      return true;
    case CASS_VALUE_TYPE_INET:  // INET can be 4 or 16 bytes, treat as variable
      return false;
    default:
      return false;
  }
}

inline int32_t fixed_size_of_type(CassValueType type) {
  switch (type) {
    case CASS_VALUE_TYPE_BOOLEAN:
    case CASS_VALUE_TYPE_TINY_INT:
      return 1;
    case CASS_VALUE_TYPE_SMALL_INT:
      return 2;
    case CASS_VALUE_TYPE_INT:
    case CASS_VALUE_TYPE_DATE:
    case CASS_VALUE_TYPE_FLOAT:
      return 4;
    case CASS_VALUE_TYPE_BIGINT:
    case CASS_VALUE_TYPE_COUNTER:
    case CASS_VALUE_TYPE_TIMESTAMP:
    case CASS_VALUE_TYPE_TIME:
    case CASS_VALUE_TYPE_DOUBLE:
      return 8;
    case CASS_VALUE_TYPE_UUID:
    case CASS_VALUE_TYPE_TIMEUUID:
      return 16;
    case CASS_VALUE_TYPE_INET:
      // This is tricky - INET can be 4 or 16 bytes
      // We'll need to handle this specially
      return -1;  // Variable
    default:
      return -1;
  }
}

}}} // namespace datastax::internal::core

#endif