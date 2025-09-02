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
#include "cassandra.h"
#include "cass_vector.hpp"
#include "vector_type.hpp"
#include "data_type.hpp"
#include "encode.hpp"
#include "string.hpp"
#include <cstring>

using namespace datastax::internal::core;
using datastax::String;

class VectorTest : public ::testing::Test {
protected:
  DataType::ConstPtr float_type;
  DataType::ConstPtr int_type;
  DataType::ConstPtr text_type;
  
  void SetUp() {
    float_type = DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_FLOAT));
    int_type = DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_INT));
    text_type = DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TEXT));
  }
  
  std::string bytes_to_hex(const char* bytes, size_t len) {
    std::string result;
    char buf[3];
    for (size_t i = 0; i < len; i++) {
      snprintf(buf, sizeof(buf), "%02X", static_cast<unsigned char>(bytes[i]));
      result += buf;
    }
    return result;
  }
};

TEST_F(VectorTest, CreateVector) {
  // Test vector creation with element type and dimension
  CassandraVector vec(float_type, 3);
  
  EXPECT_EQ(vec.dimension(), 3u);
  EXPECT_EQ(vec.size(), 0u);
  EXPECT_FALSE(vec.is_full());
  EXPECT_EQ(vec.element_type(), float_type);
}

TEST_F(VectorTest, VectorTypeCreation) {
  // Test VectorType creation
  VectorType vec_type(float_type, 3);
  
  EXPECT_EQ(vec_type.dimension(), 3u);
  EXPECT_EQ(vec_type.element_type(), float_type);
  EXPECT_TRUE(vec_type.is_fixed_length_element());
  
  // Check class name generation
  String expected = "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.FloatType, 3)";
  EXPECT_EQ(vec_type.class_name(), expected);
}

TEST_F(VectorTest, AppendElements) {
  CassandraVector vec(float_type, 3);
  
  // Append elements
  EXPECT_EQ(vec.append(1.0f), CASS_OK);
  EXPECT_EQ(vec.size(), 1u);
  EXPECT_FALSE(vec.is_full());
  
  EXPECT_EQ(vec.append(2.0f), CASS_OK);
  EXPECT_EQ(vec.size(), 2u);
  EXPECT_FALSE(vec.is_full());
  
  EXPECT_EQ(vec.append(3.0f), CASS_OK);
  EXPECT_EQ(vec.size(), 3u);
  EXPECT_TRUE(vec.is_full());
  
  // Try to append beyond dimension - should fail
  EXPECT_EQ(vec.append(4.0f), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  EXPECT_EQ(vec.size(), 3u);
}

TEST_F(VectorTest, NullNotAllowed) {
  CassandraVector vec(float_type, 3);
  
  // Vectors don't support null elements
  EXPECT_EQ(vec.append(CassNull()), CASS_ERROR_LIB_NULL_VALUE);
}

TEST_F(VectorTest, FixedLengthEncoding) {
  // Test encoding of fixed-length elements (no UVINT prefix)
  CassandraVector vec(float_type, 3);
  
  vec.append(1.0f);
  vec.append(2.0f);
  vec.append(3.0f);
  
  Buffer encoded = vec.encode();
  
  // Float vectors should be 12 bytes (3 * 4 bytes) with no size prefixes
  EXPECT_EQ(encoded.size(), 12u);
  
  // Check the encoded bytes (IEEE 754 format for 1.0, 2.0, 3.0)
  // 1.0f = 0x3F800000, 2.0f = 0x40000000, 3.0f = 0x40400000
  std::string hex = bytes_to_hex(encoded.data(), encoded.size());
  EXPECT_EQ(hex, "3F8000004000000040400000");  // Big-endian encoding
}

TEST_F(VectorTest, VariableLengthEncoding) {
  // Test encoding of variable-length elements (with UVINT prefix)
  CassandraVector vec(text_type, 2);
  
  vec.append(CassString("hello", 5));
  vec.append(CassString("world", 5));
  
  Buffer encoded = vec.encode();
  
  // Each string: UVINT(5) + 5 bytes = 1 + 5 = 6 bytes
  // Total: 6 + 6 = 12 bytes
  EXPECT_EQ(encoded.size(), 12u);
  
  // Check UVINT prefixes
  EXPECT_EQ(static_cast<uint8_t>(encoded.data()[0]), 0x05);  // UVINT(5)
  EXPECT_EQ(memcmp(encoded.data() + 1, "hello", 5), 0);
  EXPECT_EQ(static_cast<uint8_t>(encoded.data()[6]), 0x05);  // UVINT(5)
  EXPECT_EQ(memcmp(encoded.data() + 7, "world", 5), 0);
}

TEST_F(VectorTest, IsFixedLengthElement) {
  // Test fixed-length type detection
  VectorType float_vec(float_type, 3);
  EXPECT_TRUE(float_vec.is_fixed_length_element());
  
  VectorType int_vec(int_type, 3);
  EXPECT_TRUE(int_vec.is_fixed_length_element());
  
  VectorType text_vec(text_type, 3);
  EXPECT_FALSE(text_vec.is_fixed_length_element());
  
  // Test other fixed-length types
  DataType::ConstPtr bigint_type(new DataType(CASS_VALUE_TYPE_BIGINT));
  VectorType bigint_vec(bigint_type, 3);
  EXPECT_TRUE(bigint_vec.is_fixed_length_element());
  
  DataType::ConstPtr boolean_type(new DataType(CASS_VALUE_TYPE_BOOLEAN));
  VectorType bool_vec(boolean_type, 3);
  EXPECT_TRUE(bool_vec.is_fixed_length_element());
  
  DataType::ConstPtr double_type(new DataType(CASS_VALUE_TYPE_DOUBLE));
  VectorType double_vec(double_type, 3);
  EXPECT_TRUE(double_vec.is_fixed_length_element());
  
  DataType::ConstPtr uuid_type(new DataType(CASS_VALUE_TYPE_UUID));
  VectorType uuid_vec(uuid_type, 3);
  EXPECT_TRUE(uuid_vec.is_fixed_length_element());
  
  DataType::ConstPtr timeuuid_type(new DataType(CASS_VALUE_TYPE_TIMEUUID));
  VectorType timeuuid_vec(timeuuid_type, 3);
  EXPECT_TRUE(timeuuid_vec.is_fixed_length_element());
  
  DataType::ConstPtr timestamp_type(new DataType(CASS_VALUE_TYPE_TIMESTAMP));
  VectorType timestamp_vec(timestamp_type, 3);
  EXPECT_TRUE(timestamp_vec.is_fixed_length_element());
}

TEST_F(VectorTest, DimensionLimits) {
  // Test dimension validation (1-8192)
  VectorType vec1(float_type, 1);
  EXPECT_EQ(vec1.dimension(), 1u);
  
  VectorType vec8192(float_type, 8192);
  EXPECT_EQ(vec8192.dimension(), 8192u);
  
  // Test actual usage
  CassandraVector small_vec(float_type, 1);
  EXPECT_EQ(small_vec.append(1.0f), CASS_OK);
  EXPECT_TRUE(small_vec.is_full());
  
  CassandraVector large_vec(float_type, 1000);
  for (int i = 0; i < 1000; i++) {
    EXPECT_EQ(large_vec.append(static_cast<float>(i)), CASS_OK);
  }
  EXPECT_TRUE(large_vec.is_full());
  EXPECT_EQ(large_vec.append(1000.0f), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
}

TEST_F(VectorTest, ClearVector) {
  CassandraVector vec(int_type, 3);
  
  vec.append(1);
  vec.append(2);
  EXPECT_EQ(vec.size(), 2u);
  
  vec.clear();
  EXPECT_EQ(vec.size(), 0u);
  EXPECT_FALSE(vec.is_full());
  
  // Can append again after clear
  EXPECT_EQ(vec.append(10), CASS_OK);
  EXPECT_EQ(vec.size(), 1u);
}

TEST_F(VectorTest, EncodingWithLength) {
  CassandraVector vec(float_type, 2);
  vec.append(1.5f);
  vec.append(2.5f);
  
  Buffer encoded_with_length = vec.encode_with_length();
  
  // Should have 4-byte length prefix + 8 bytes of data
  EXPECT_EQ(encoded_with_length.size(), 12u);
  
  // Check length prefix (8 in big-endian)
  int32_t length;
  memcpy(&length, encoded_with_length.data(), sizeof(int32_t));
  // Need to convert from network byte order
  length = ntohl(length);
  EXPECT_EQ(length, 8);
}

TEST_F(VectorTest, VectorTypeToString) {
  VectorType float_vec(float_type, 3);
  EXPECT_EQ(float_vec.to_string(), "vector<float, 3>");
  
  VectorType int_vec(int_type, 10);
  EXPECT_EQ(int_vec.to_string(), "vector<int, 10>");
  
  VectorType text_vec(text_type, 5);
  EXPECT_EQ(text_vec.to_string(), "vector<text, 5>");
}

TEST_F(VectorTest, VectorTypeCopy) {
  VectorType original(float_type, 3);
  DataType::Ptr copy = original.copy();
  
  VectorType* vec_copy = static_cast<VectorType*>(copy.get());
  EXPECT_EQ(vec_copy->dimension(), 3u);
  EXPECT_EQ(vec_copy->element_type(), float_type);
  EXPECT_EQ(vec_copy->class_name(), original.class_name());
}