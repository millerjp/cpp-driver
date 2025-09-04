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

#include "decoder.hpp"
#include "logger.hpp"
#include "value.hpp"
#include "uvint.hpp"

#define CHECK_REMAINING(SIZE, DETAIL)             \
  do {                                            \
    if (remaining_ < static_cast<size_t>(SIZE)) { \
      notify_error(DETAIL, SIZE);                 \
      return false;                               \
    }                                             \
  } while (0)

using namespace datastax::internal::core;

void Decoder::maybe_log_remaining() const {
  if (remaining_ > 0) {
    LOG_TRACE("Data remaining in %s response: %u", type_, static_cast<unsigned int>(remaining_));
  }
}

bool Decoder::decode_inet(Address* output) {
  CHECK_REMAINING(sizeof(uint8_t), "length of inet");

  uint8_t address_length = 0;
  input_ = internal::decode_byte(input_, address_length);
  remaining_ -= sizeof(uint8_t);
  if (address_length > CASS_INET_V6_LENGTH) {
    LOG_ERROR("Invalid inet address length of %d bytes", address_length);
    return false;
  }

  CHECK_REMAINING(address_length, "inet");
  uint8_t address[CASS_INET_V6_LENGTH];
  memcpy(address, input_, address_length);
  input_ += address_length;
  remaining_ -= address_length;

  CHECK_REMAINING(sizeof(int32_t), "port");
  int32_t port = 0;
  input_ = internal::decode_int32(input_, port);
  remaining_ -= sizeof(int32_t);

  *output = Address(address, address_length, port);
  return output->is_valid_and_resolved();
}

bool Decoder::decode_inet(CassInet* output) {
  CHECK_REMAINING(sizeof(uint8_t), "length of inet");

  input_ = internal::decode_byte(input_, output->address_length);
  remaining_ -= sizeof(uint8_t);
  if (output->address_length > CASS_INET_V6_LENGTH) {
    LOG_ERROR("Invalid inet address length of %d bytes", output->address_length);
    return false;
  }

  CHECK_REMAINING(output->address_length, "inet");
  memcpy(output->address, input_, output->address_length);
  input_ += output->address_length;
  remaining_ -= output->address_length;
  return true;
}

bool Decoder::as_inet(const int address_length, CassInet* output) const {
  output->address_length = static_cast<uint8_t>(address_length);
  if (output->address_length > CASS_INET_V6_LENGTH) {
    LOG_ERROR("Invalid inet address length of %d bytes", output->address_length);
    return false;
  }

  CHECK_REMAINING(output->address_length, "inet");
  memcpy(output->address, input_, output->address_length);
  return true;
}

bool Decoder::decode_write_type(CassWriteType& output) {
  StringRef write_type;
  output = CASS_WRITE_TYPE_UNKNOWN;
  if (!decode_string(&write_type)) return false;

  if (write_type == "SIMPLE") {
    output = CASS_WRITE_TYPE_SIMPLE;
  } else if (write_type == "BATCH") {
    output = CASS_WRITE_TYPE_BATCH;
  } else if (write_type == "UNLOGGED_BATCH") {
    output = CASS_WRITE_TYPE_UNLOGGED_BATCH;
  } else if (write_type == "COUNTER") {
    output = CASS_WRITE_TYPE_COUNTER;
  } else if (write_type == "BATCH_LOG") {
    output = CASS_WRITE_TYPE_BATCH_LOG;
  } else if (write_type == "CAS") {
    output = CASS_WRITE_TYPE_CAS;
  } else if (write_type == "VIEW") {
    output = CASS_WRITE_TYPE_VIEW;
  } else if (write_type == "CDC") {
    output = CASS_WRITE_TYPE_CDC;
  } else {
    LOG_WARN("Invalid write type %.*s", (int)write_type.size(), write_type.data());
    return false;
  }

  return true;
}

bool Decoder::decode_warnings(WarningVec& output) {
  if (remaining_ < sizeof(uint16_t)) {
    notify_error("count of warnings", sizeof(uint16_t));
    return false;
  }
  uint16_t count = 0;
  input_ = internal::decode_uint16(input_, count);
  remaining_ -= sizeof(uint16_t);

  for (uint16_t i = 0; i < count; ++i) {
    StringRef warning;

    if (!decode_string(&warning)) return false;
    LOG_WARN("Server-side warning: %.*s", (int)warning.size(), warning.data());
    output.push_back(warning);
  }

  return true;
}

Value Decoder::decode_value(const DataType::ConstPtr& data_type) {
  int32_t size = 0;
  if (!decode_int32(size)) return Value();

  if (size >= 0) {
    Decoder decoder(input_, size, protocol_version_);
    input_ += size;
    remaining_ -= size;

    int32_t count = 0;
    if (!data_type->is_collection()) {
      return Value(data_type, decoder);
    } else if (decoder.decode_int32(count)) {
      return Value(data_type, count, decoder);
    }
    return Value();
  }
  return Value(data_type);
}

Value Decoder::decode_vector_element(const DataType::ConstPtr& element_type, bool is_fixed_length) {
  if (is_fixed_length) {
    // Fixed-length types have no size prefix in vectors
    // Determine the size based on the type
    size_t element_size = 0;
    switch (element_type->value_type()) {
      case CASS_VALUE_TYPE_BOOLEAN:
        element_size = 1;
        break;
      case CASS_VALUE_TYPE_INT:
      case CASS_VALUE_TYPE_FLOAT:
        element_size = 4;
        break;
      case CASS_VALUE_TYPE_BIGINT:
      case CASS_VALUE_TYPE_TIMESTAMP:
      case CASS_VALUE_TYPE_DOUBLE:
        element_size = 8;
        break;
      case CASS_VALUE_TYPE_UUID:
      case CASS_VALUE_TYPE_TIMEUUID:
        element_size = 16;
        break;
      default:
        // Unknown fixed-length type
        return Value();
    }
    
    if (remaining_ < element_size) {
      return Value(); // Not enough data
    }
    
    Decoder element_decoder(input_, element_size, protocol_version_);
    input_ += element_size;
    remaining_ -= element_size;
    return Value(element_type, element_decoder);
  } else {
    // Variable-length types have UVINT size prefix in vectors
    uint64_t size = 0;
    size_t uvint_bytes = decode_uvint(reinterpret_cast<const uint8_t*>(input_), 
                                     remaining_, &size);
    if (uvint_bytes == 0 || uvint_bytes > remaining_) {
      return Value(); // Failed to decode UVINT
    }
    
    size_t element_size = static_cast<size_t>(size);
    if (uvint_bytes + element_size > remaining_) {
      return Value(); // Not enough data
    }
    
    // Skip UVINT prefix and create decoder for element data
    Decoder element_decoder(input_ + uvint_bytes, element_size, protocol_version_);
    input_ += uvint_bytes + element_size;
    remaining_ -= uvint_bytes + element_size;
    
    // For collections and other complex types, they have their own internal structure
    if (element_type->is_collection()) {
      int32_t count = 0;
      if (element_decoder.decode_int32(count)) {
        return Value(element_type, count, element_decoder);
      }
      return Value();
    } else if (element_type->is_custom()) {
      // Special handling for "unknown" types - they might be collections
      const CustomType* custom = static_cast<const CustomType*>(element_type.get());
      if (custom && custom->class_name() == "unknown") {
        // For "unknown" types, we need to peek at the data to see if it's a collection
        // Collections start with an int32 count
        int32_t count = 0;
        Decoder test_decoder(element_decoder);
        if (test_decoder.decode_int32(count) && count >= 0 && count < 1000000) {
          // Looks like a collection - treat it as one
          return Value(element_type, count, element_decoder);
        }
      }
      return Value(element_type, element_decoder);
    } else {
      return Value(element_type, element_decoder);
    }
  }
}

bool Decoder::update_value(Value& value) {
  int32_t size = 0;
  if (decode_int32(size)) {
    if (size >= 0) {
      Decoder decoder(input_, size, protocol_version_);
      input_ += size;
      remaining_ -= size;
      return value.update(decoder);
    }
    Decoder decoder;
    return value.update(decoder);
  }
  return false;
}

void Decoder::notify_error(const char* detail, size_t bytes) const {
  if (strlen(type_) == 0) {
    LOG_ERROR("Expected at least %u byte%s to decode %s value", static_cast<unsigned int>(bytes),
              (bytes > 1 ? "s" : ""), detail);
  } else {
    LOG_ERROR("Expected at least %u byte%s to decode %s %s response",
              static_cast<unsigned int>(bytes), (bytes > 1 ? "s" : ""), detail, type_);
  }
}
