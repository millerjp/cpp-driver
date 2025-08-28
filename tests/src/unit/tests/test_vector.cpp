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
#include "vector_value.hpp"
#include "data_type.hpp"
#include "value.hpp"
#include "serialization.hpp"
#include "vector_iterator.hpp"
#include "encode.hpp"

using namespace datastax::internal::core;
using namespace datastax;

class VectorUnitTest : public ::testing::Test {
protected:
  void SetUp() {
    // Don't create a default vector here anymore
    // Each test will create its own with the appropriate type
  }

  void TearDown() {
    // Cleanup handled in individual tests
  }
};

TEST(VectorBasicsTest, VectorTypeEnumDefined) {
  // Verify the vector type enum value exists and has the expected value
  EXPECT_EQ(0x0023, CASS_VALUE_TYPE_VECTOR);
  
  // Verify it comes after SET (0x0022) and before UDT (0x0030)
  EXPECT_GT(CASS_VALUE_TYPE_VECTOR, CASS_VALUE_TYPE_SET);
  EXPECT_LT(CASS_VALUE_TYPE_VECTOR, CASS_VALUE_TYPE_UDT);
}

TEST_F(VectorUnitTest, CreateAndDestroy) {
  CassVector* v = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 5);
  ASSERT_NE(v, nullptr);
  cass_vector_free(v);
  // Test that freeing NULL doesn't crash
  cass_vector_free(NULL);
}

TEST_F(VectorUnitTest, AppendFloat) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  EXPECT_EQ(cass_vector_append_float(vector, 1.5f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector, 2.5f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector, 3.5f), CASS_OK);
  // Should fail when exceeding dimension
  EXPECT_EQ(cass_vector_append_float(vector, 4.5f), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendDouble) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_DOUBLE, 3);
  EXPECT_EQ(cass_vector_append_double(vector, 1.5), CASS_OK);
  EXPECT_EQ(cass_vector_append_double(vector, 2.5), CASS_OK);
  EXPECT_EQ(cass_vector_append_double(vector, 3.5), CASS_OK);
  // Should fail when exceeding dimension
  EXPECT_EQ(cass_vector_append_double(vector, 4.5), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendInt32) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_INT, 3);
  EXPECT_EQ(cass_vector_append_int32(vector, 1), CASS_OK);
  EXPECT_EQ(cass_vector_append_int32(vector, 2), CASS_OK);
  EXPECT_EQ(cass_vector_append_int32(vector, 3), CASS_OK);
  // Should fail when exceeding dimension
  EXPECT_EQ(cass_vector_append_int32(vector, 4), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendInt64) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BIGINT, 3);
  EXPECT_EQ(cass_vector_append_int64(vector, 100000000000LL), CASS_OK);
  EXPECT_EQ(cass_vector_append_int64(vector, 200000000000LL), CASS_OK);
  EXPECT_EQ(cass_vector_append_int64(vector, 300000000000LL), CASS_OK);
  // Should fail when exceeding dimension
  EXPECT_EQ(cass_vector_append_int64(vector, 400000000000LL), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendInt8) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TINY_INT, 3);
  EXPECT_EQ(cass_vector_append_int8(vector, 1), CASS_OK);
  EXPECT_EQ(cass_vector_append_int8(vector, 2), CASS_OK);
  EXPECT_EQ(cass_vector_append_int8(vector, 3), CASS_OK);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendInt16) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_SMALL_INT, 3);
  EXPECT_EQ(cass_vector_append_int16(vector, 1000), CASS_OK);
  EXPECT_EQ(cass_vector_append_int16(vector, 2000), CASS_OK);
  EXPECT_EQ(cass_vector_append_int16(vector, 3000), CASS_OK);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendString) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TEXT, 3);
  EXPECT_EQ(cass_vector_append_string(vector, "hello"), CASS_OK);
  EXPECT_EQ(cass_vector_append_string_n(vector, "world", 5), CASS_OK);
  EXPECT_EQ(cass_vector_append_string(vector, "test"), CASS_OK);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendBytes) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BLOB, 3);
  const cass_byte_t bytes1[] = {0x01, 0x02, 0x03};
  const cass_byte_t bytes2[] = {0x04, 0x05};
  const cass_byte_t bytes3[] = {0x06, 0x07, 0x08, 0x09};
  
  EXPECT_EQ(cass_vector_append_bytes(vector, bytes1, sizeof(bytes1)), CASS_OK);
  EXPECT_EQ(cass_vector_append_bytes(vector, bytes2, sizeof(bytes2)), CASS_OK);
  EXPECT_EQ(cass_vector_append_bytes(vector, bytes3, sizeof(bytes3)), CASS_OK);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendUuid) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_UUID, 3);
  CassUuid uuid1 = {123456789, 987654321};
  CassUuid uuid2 = {111111111, 222222222};
  CassUuid uuid3 = {333333333, 444444444};
  
  EXPECT_EQ(cass_vector_append_uuid(vector, uuid1), CASS_OK);
  EXPECT_EQ(cass_vector_append_uuid(vector, uuid2), CASS_OK);
  EXPECT_EQ(cass_vector_append_uuid(vector, uuid3), CASS_OK);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendBool) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BOOLEAN, 3);
  EXPECT_EQ(cass_vector_append_bool(vector, cass_true), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(vector, cass_false), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(vector, cass_true), CASS_OK);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendTuple) {
  // Note: Tuples in vectors are not supported by Cassandra 5.0
  // Creating a vector with TUPLE element type should return NULL
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TUPLE, 3);
  EXPECT_EQ(vector, nullptr) << "Should return NULL for unsupported TUPLE element type";
  
  // Test is complete - Tuple vectors cannot be created
}

TEST_F(VectorUnitTest, AppendCollection) {
  // Note: Collections in vectors are not supported by Cassandra 5.0
  // Creating a vector with LIST element type should return NULL
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_LIST, 3);
  EXPECT_EQ(vector, nullptr) << "Should return NULL for unsupported LIST element type";
  
  // Also test SET and MAP types
  vector = cass_vector_new(CASS_VALUE_TYPE_SET, 3);
  EXPECT_EQ(vector, nullptr) << "Should return NULL for unsupported SET element type";
  
  vector = cass_vector_new(CASS_VALUE_TYPE_MAP, 3);
  EXPECT_EQ(vector, nullptr) << "Should return NULL for unsupported MAP element type";
}

TEST_F(VectorUnitTest, AppendUserType) {
  // Note: UDTs in vectors are not supported by Cassandra 5.0
  // Creating a vector with UDT element type should return NULL
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_UDT, 3);
  EXPECT_EQ(vector, nullptr) << "Should return NULL for unsupported UDT element type";
  
  // Test is complete - UDT vectors cannot be created
}

// Note: cass_vector_append_null doesn't exist - vectors have fixed types

TEST_F(VectorUnitTest, AppendInet) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_INET, 3);
  CassInet inet1, inet2, inet3;
  ASSERT_EQ(cass_inet_from_string("127.0.0.1", &inet1), CASS_OK);
  ASSERT_EQ(cass_inet_from_string("192.168.1.1", &inet2), CASS_OK);
  ASSERT_EQ(cass_inet_from_string("::1", &inet3), CASS_OK);
  
  // Note: INET vectors are broken in Cassandra 5.0
  EXPECT_EQ(cass_vector_append_inet(vector, inet1), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
  EXPECT_EQ(cass_vector_append_inet(vector, inet2), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
  EXPECT_EQ(cass_vector_append_inet(vector, inet3), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendDecimal) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_DECIMAL, 3);
  const cass_byte_t varint1[] = {0x01, 0x02};
  const cass_byte_t varint2[] = {0x03, 0x04};
  const cass_byte_t varint3[] = {0x05, 0x06};
  
  EXPECT_EQ(cass_vector_append_decimal(vector, varint1, sizeof(varint1), 2), CASS_OK);
  EXPECT_EQ(cass_vector_append_decimal(vector, varint2, sizeof(varint2), 3), CASS_OK);
  EXPECT_EQ(cass_vector_append_decimal(vector, varint3, sizeof(varint3), 4), CASS_OK);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendDuration) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_DURATION, 3);
  // Note: Duration vectors are broken in Cassandra 5.0
  EXPECT_EQ(cass_vector_append_duration(vector, 1, 2, 3), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
  EXPECT_EQ(cass_vector_append_duration(vector, 4, 5, 6), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
  EXPECT_EQ(cass_vector_append_duration(vector, 7, 8, 9), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, AppendVector) {
  // Note: Nested vectors are not supported by Cassandra 5.0
  // Creating a vector with VECTOR element type should return NULL
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_VECTOR, 3);
  EXPECT_EQ(vector, nullptr) << "Should return NULL for unsupported VECTOR element type";
  
  // Test is complete - Vector of vectors cannot be created
}

TEST_F(VectorUnitTest, Serialization) {
  // Test float vector serialization
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  cass_vector_append_float(vector, 1.0f);
  cass_vector_append_float(vector, 2.0f);
  cass_vector_append_float(vector, 3.0f);
  
  VectorValue* vector_value = static_cast<VectorValue*>(vector->from());
  Buffer encoded = vector_value->encode();
  
  // Verify encoding format
  EXPECT_GT(encoded.size(), static_cast<size_t>(0));
  
  // For floats, each is 4 bytes
  // Expected: 3 floats * 4 bytes = 12 bytes
  EXPECT_EQ(encoded.size(), static_cast<size_t>(12));
  
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, SerializationWithLength) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_INT, 3);
  cass_vector_append_int32(vector, 100);
  cass_vector_append_int32(vector, 200);
  cass_vector_append_int32(vector, 300);
  
  VectorValue* vector_value = static_cast<VectorValue*>(vector->from());
  Buffer encoded = vector_value->encode_with_length();
  
  // Should have 4-byte length prefix followed by data
  EXPECT_GT(encoded.size(), static_cast<size_t>(4));
  
  cass_vector_free(vector);
}

TEST_F(VectorUnitTest, TypeMismatch) {
  // Create a float vector but try to append wrong types
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  
  // Try to append int32 to a float vector - should fail
  EXPECT_EQ(cass_vector_append_int32(vector, 100), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
  
  // Try to append string to a float vector - should fail
  EXPECT_EQ(cass_vector_append_string(vector, "test"), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
  
  // Correct type should work
  EXPECT_EQ(cass_vector_append_float(vector, 1.0f), CASS_OK);
  
  cass_vector_free(vector);
}


TEST(VectorTypeTest, Creation) {
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  VectorType vector_type(float_type, 128);
  
  EXPECT_EQ(vector_type.value_type(), CASS_VALUE_TYPE_VECTOR);
  EXPECT_EQ(vector_type.element_type()->value_type(), CASS_VALUE_TYPE_FLOAT);
  EXPECT_EQ(vector_type.dimension(), static_cast<size_t>(128));
}

TEST(VectorTypeTest, Comparison) {
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  
  DataType::ConstPtr vector1(new VectorType(float_type, 128));
  DataType::ConstPtr vector2(new VectorType(float_type, 128));
  DataType::ConstPtr vector3(new VectorType(float_type, 256));
  DataType::ConstPtr vector4(new VectorType(int_type, 128));
  
  // Same type and dimension
  EXPECT_TRUE(vector1->equals(vector2));
  
  // Different dimension
  EXPECT_FALSE(vector1->equals(vector3));
  
  // Different element type
  EXPECT_FALSE(vector1->equals(vector4));
}

TEST(VectorIteratorTest, Iteration) {
  // Create a vector with float elements
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  DataType::ConstPtr vector_type(new VectorType(float_type, 3));
  
  // Create encoded data for 3 floats
  char buffer[12]; // 3 floats * 4 bytes
  char* pos = buffer;
  pos = datastax::internal::encode_float(pos, 1.0f);
  pos = datastax::internal::encode_float(pos, 2.0f);
  pos = datastax::internal::encode_float(pos, 3.0f);
  
  // Create a Value from the encoded data
  Value vector_value(vector_type, Decoder(buffer, sizeof(buffer)));
  
  // Create iterator
  VectorIterator iter(&vector_value);
  
  // Iterate through elements
  int count = 0;
  while (iter.next()) {
    const Value* element = iter.value();
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->value_type(), CASS_VALUE_TYPE_FLOAT);
    count++;
  }
  
  EXPECT_EQ(count, 3);
}

TEST(VectorIteratorTest, EmptyVector) {
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  DataType::ConstPtr vector_type(new VectorType(float_type, 0));
  
  char empty_buffer[1] = {0}; // Need at least 1 byte
  Value vector_value(vector_type, Decoder(empty_buffer, 0));
  
  VectorIterator iter(&vector_value);
  
  // Should not iterate at all
  EXPECT_FALSE(iter.next());
}

TEST(ValueTest, IsVector) {
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  DataType::ConstPtr vector_type(new VectorType(float_type, 3));
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  
  char dummy_buffer[1] = {0};
  Value vector_value(vector_type, Decoder(dummy_buffer, 0));
  Value int_value(int_type, Decoder(dummy_buffer, 0));
  
  EXPECT_TRUE(vector_value.is_vector());
  EXPECT_FALSE(int_value.is_vector());
}

TEST(ValueTest, VectorCount) {
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  DataType::ConstPtr vector_type(new VectorType(float_type, 128));
  
  char dummy_buffer[1] = {0};
  Value vector_value(vector_type, Decoder(dummy_buffer, 0));
  
  // Count should be set to dimension
  EXPECT_EQ(vector_value.count(), static_cast<int32_t>(128));
}