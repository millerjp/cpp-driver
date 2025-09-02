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

#include <gtest/gtest.h>
#include "uvint.hpp"
#include <vector>
#include <cstring>

using namespace datastax::internal::core;

// Test vector structure for validation against Go driver
struct UvintTestVector {
  uint64_t value;
  std::vector<uint8_t> expected_bytes;
  const char* description;
};

class UvintTest : public ::testing::Test {
protected:
  // Test vectors from Go driver documentation and implementation
  std::vector<UvintTestVector> test_vectors = {
    // Single byte (0-127)
    {0,     {0x00},           "Zero value"},
    {1,     {0x01},           "One"},
    {127,   {0x7F},           "Max single byte"},
    
    // Two bytes (128-16383)
    {128,   {0x80, 0x80},     "Min two bytes"},
    {255,   {0x80, 0xFF},     "255 in two bytes"},
    {256,   {0x81, 0x00},     "256 in two bytes"},
    {16383, {0xBF, 0xFF},     "Max two bytes"},
    
    // Three bytes (16384-2097151)
    {16384,  {0xC0, 0x40, 0x00}, "Min three bytes"},
    {256000, {0xC3, 0xE8, 0x00}, "256000 from Go test"},
    {2097151, {0xDF, 0xFF, 0xFF}, "Max three bytes"},
    
    // Four bytes
    {2097152, {0xE0, 0x20, 0x00, 0x00}, "Min four bytes"},
    {16777215, {0xE0, 0xFF, 0xFF, 0xFF}, "Max four bytes"},
    
    // Five bytes
    {16777216, {0xE1, 0x00, 0x00, 0x00}, "Min five bytes (actually 4 bytes)"},
    {4294967295ULL, {0xF0, 0xFF, 0xFF, 0xFF, 0xFF}, "Max uint32"},
    
    // Edge cases
    {65535, {0xC0, 0xFF, 0xFF}, "Max uint16"},
    {65536, {0xC1, 0x00, 0x00}, "Max uint16 + 1"},
    
    // Large values
    {1099511627775ULL, {0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, "2^40 - 1"},
    {0xFFFFFFFFFFFFULL, {0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, "2^48 - 1"},
  };
  
  std::string bytes_to_hex(const uint8_t* bytes, size_t len) {
    std::string result;
    char buf[3];
    for (size_t i = 0; i < len; i++) {
      snprintf(buf, sizeof(buf), "%02X", bytes[i]);
      if (i > 0) result += " ";
      result += buf;
    }
    return result;
  }
};

TEST_F(UvintTest, SizeCalculation) {
  // Test size calculation for all test vectors
  for (const auto& tv : test_vectors) {
    size_t calculated_size = uvint_size(tv.value);
    EXPECT_EQ(calculated_size, tv.expected_bytes.size()) 
      << "Size mismatch for value " << tv.value 
      << " (" << tv.description << ")"
      << " - expected " << tv.expected_bytes.size() 
      << ", got " << calculated_size;
  }
}

TEST_F(UvintTest, Encoding) {
  // Test encoding for all test vectors
  for (const auto& tv : test_vectors) {
    uint8_t buffer[9] = {0}; // Max 9 bytes for uint64
    size_t bytes_written = encode_uvint(tv.value, buffer);
    
    // Check number of bytes written
    EXPECT_EQ(bytes_written, tv.expected_bytes.size()) 
      << "Encoding size mismatch for value " << tv.value 
      << " (" << tv.description << ")";
    
    // Check byte-by-byte match with expected
    bool bytes_match = (memcmp(buffer, tv.expected_bytes.data(), bytes_written) == 0);
    
    EXPECT_TRUE(bytes_match)
      << "Encoding mismatch for value " << tv.value 
      << " (" << tv.description << ")"
      << "\nExpected: " << bytes_to_hex(tv.expected_bytes.data(), tv.expected_bytes.size())
      << "\nGot:      " << bytes_to_hex(buffer, bytes_written);
  }
}

TEST_F(UvintTest, Decoding) {
  // Test decoding for all test vectors
  for (const auto& tv : test_vectors) {
    uint64_t decoded_value = 0;
    size_t bytes_consumed = decode_uvint(tv.expected_bytes.data(), 
                                         tv.expected_bytes.size(), 
                                         &decoded_value);
    
    // Check number of bytes consumed
    EXPECT_EQ(bytes_consumed, tv.expected_bytes.size())
      << "Decoding size mismatch for " << tv.description;
    
    // Check decoded value matches original
    EXPECT_EQ(decoded_value, tv.value)
      << "Decoding value mismatch for " << tv.description
      << " - expected " << tv.value << ", got " << decoded_value;
  }
}

TEST_F(UvintTest, RoundTrip) {
  // Test encode->decode round trip for various values
  std::vector<uint64_t> test_values = {
    0, 1, 127, 128, 255, 256, 16383, 16384, 65535, 65536,
    1000000, 10000000, 100000000, 1000000000,
    0xFFFFFFFF, 0x100000000ULL, 0xFFFFFFFFFFFFULL
  };
  
  for (uint64_t value : test_values) {
    uint8_t buffer[9] = {0};
    size_t encoded_size = encode_uvint(value, buffer);
    
    uint64_t decoded_value = 0;
    size_t decoded_size = decode_uvint(buffer, encoded_size, &decoded_value);
    
    EXPECT_EQ(encoded_size, decoded_size)
      << "Size mismatch in round trip for value " << value;
    EXPECT_EQ(value, decoded_value)
      << "Value mismatch in round trip for value " << value;
  }
}

TEST_F(UvintTest, DecodingErrors) {
  // Test error cases
  uint64_t value = 0;
  
  // Empty buffer
  EXPECT_EQ(decode_uvint(nullptr, 0, &value), 0u);
  
  // Null value pointer
  uint8_t buffer[] = {0x01};
  EXPECT_EQ(decode_uvint(buffer, 1, nullptr), 0u);
  
  // Truncated multi-byte encoding
  uint8_t truncated[] = {0x80}; // Should be 2 bytes
  EXPECT_EQ(decode_uvint(truncated, 1, &value), 0u);
  
  uint8_t truncated3[] = {0xC0, 0x40}; // Should be 3 bytes
  EXPECT_EQ(decode_uvint(truncated3, 2, &value), 0u);
}

TEST_F(UvintTest, BoundaryValues) {
  // Test boundary values for each byte count
  struct BoundaryTest {
    uint64_t value;
    size_t expected_bytes;
  };
  
  std::vector<BoundaryTest> boundaries = {
    {0, 1},                    // Min 1 byte
    {127, 1},                  // Max 1 byte
    {128, 2},                  // Min 2 bytes
    {16383, 2},                // Max 2 bytes
    {16384, 3},                // Min 3 bytes
    {2097151, 3},              // Max 3 bytes
    {2097152, 4},              // Min 4 bytes
    {268435455, 4},            // Max 4 bytes
    {268435456, 5},            // Min 5 bytes
  };
  
  for (const auto& bt : boundaries) {
    EXPECT_EQ(uvint_size(bt.value), bt.expected_bytes)
      << "Boundary size check failed for value " << bt.value;
    
    uint8_t buffer[9] = {0};
    size_t encoded = encode_uvint(bt.value, buffer);
    EXPECT_EQ(encoded, bt.expected_bytes)
      << "Boundary encoding size failed for value " << bt.value;
    
    uint64_t decoded = 0;
    size_t consumed = decode_uvint(buffer, encoded, &decoded);
    EXPECT_EQ(consumed, bt.expected_bytes)
      << "Boundary decoding size failed for value " << bt.value;
    EXPECT_EQ(decoded, bt.value)
      << "Boundary decoding value failed for value " << bt.value;
  }
}

// Test against specific Go driver test vectors from implementation plan
TEST_F(UvintTest, GoDriverCompatibility) {
  // These are the exact test vectors from the Go driver documentation
  struct GoTestVector {
    uint64_t value;
    const char* hex_bytes;
  };
  
  std::vector<GoTestVector> go_vectors = {
    {0,      "00"},
    {127,    "7F"},
    {128,    "8080"},
    {255,    "80FF"},
    {256000, "C3E800"},
  };
  
  for (const auto& gtv : go_vectors) {
    uint8_t buffer[9] = {0};
    size_t size = encode_uvint(gtv.value, buffer);
    
    std::string actual_hex = bytes_to_hex(buffer, size);
    std::string expected_hex = gtv.hex_bytes;
    
    // Remove spaces for comparison
    actual_hex.erase(std::remove(actual_hex.begin(), actual_hex.end(), ' '), actual_hex.end());
    
    EXPECT_EQ(actual_hex, expected_hex)
      << "Go driver compatibility failed for value " << gtv.value
      << "\nExpected (Go): " << expected_hex
      << "\nGot (C++):     " << actual_hex;
  }
}