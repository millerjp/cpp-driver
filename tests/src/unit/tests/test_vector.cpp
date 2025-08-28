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
    vector_ = cass_vector_new(3);
  }

  void TearDown() {
    if (vector_) {
      cass_vector_free(vector_);
    }
  }

  CassVector* vector_;
};

TEST(VectorBasicsTest, VectorTypeEnumDefined) {
  // Verify the vector type enum value exists and has the expected value
  EXPECT_EQ(0x0023, CASS_VALUE_TYPE_VECTOR);
  
  // Verify it comes after SET (0x0022) and before UDT (0x0030)
  EXPECT_GT(CASS_VALUE_TYPE_VECTOR, CASS_VALUE_TYPE_SET);
  EXPECT_LT(CASS_VALUE_TYPE_VECTOR, CASS_VALUE_TYPE_UDT);
}

TEST_F(VectorUnitTest, CreateAndDestroy) {
  CassVector* v = cass_vector_new(5);
  ASSERT_NE(v, nullptr);
  cass_vector_free(v);
  // Test that freeing NULL doesn't crash
  cass_vector_free(NULL);
}

TEST_F(VectorUnitTest, AppendFloat) {
  EXPECT_EQ(cass_vector_append_float(vector_, 1.5f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector_, 2.5f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector_, 3.5f), CASS_OK);
  // Should fail when exceeding dimension
  EXPECT_EQ(cass_vector_append_float(vector_, 4.5f), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
}

TEST_F(VectorUnitTest, AppendDouble) {
  EXPECT_EQ(cass_vector_append_double(vector_, 1.5), CASS_OK);
  EXPECT_EQ(cass_vector_append_double(vector_, 2.5), CASS_OK);
  EXPECT_EQ(cass_vector_append_double(vector_, 3.5), CASS_OK);
  // Should fail when exceeding dimension
  EXPECT_EQ(cass_vector_append_double(vector_, 4.5), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
}

TEST_F(VectorUnitTest, AppendInt32) {
  EXPECT_EQ(cass_vector_append_int32(vector_, 1), CASS_OK);
  EXPECT_EQ(cass_vector_append_int32(vector_, 2), CASS_OK);
  EXPECT_EQ(cass_vector_append_int32(vector_, 3), CASS_OK);
  // Should fail when exceeding dimension
  EXPECT_EQ(cass_vector_append_int32(vector_, 4), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
}

TEST_F(VectorUnitTest, AppendInt64) {
  EXPECT_EQ(cass_vector_append_int64(vector_, 100000000000LL), CASS_OK);
  EXPECT_EQ(cass_vector_append_int64(vector_, 200000000000LL), CASS_OK);
  EXPECT_EQ(cass_vector_append_int64(vector_, 300000000000LL), CASS_OK);
  // Should fail when exceeding dimension
  EXPECT_EQ(cass_vector_append_int64(vector_, 400000000000LL), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
}

TEST_F(VectorUnitTest, AppendInt8) {
  EXPECT_EQ(cass_vector_append_int8(vector_, 1), CASS_OK);
  EXPECT_EQ(cass_vector_append_int8(vector_, 2), CASS_OK);
  EXPECT_EQ(cass_vector_append_int8(vector_, 3), CASS_OK);
}

TEST_F(VectorUnitTest, AppendInt16) {
  EXPECT_EQ(cass_vector_append_int16(vector_, 1000), CASS_OK);
  EXPECT_EQ(cass_vector_append_int16(vector_, 2000), CASS_OK);
  EXPECT_EQ(cass_vector_append_int16(vector_, 3000), CASS_OK);
}

TEST_F(VectorUnitTest, AppendString) {
  EXPECT_EQ(cass_vector_append_string(vector_, "hello"), CASS_OK);
  EXPECT_EQ(cass_vector_append_string_n(vector_, "world", 5), CASS_OK);
  EXPECT_EQ(cass_vector_append_string(vector_, "test"), CASS_OK);
}

TEST_F(VectorUnitTest, AppendBytes) {
  const cass_byte_t bytes1[] = {0x01, 0x02, 0x03};
  const cass_byte_t bytes2[] = {0x04, 0x05};
  const cass_byte_t bytes3[] = {0x06, 0x07, 0x08, 0x09};
  
  EXPECT_EQ(cass_vector_append_bytes(vector_, bytes1, sizeof(bytes1)), CASS_OK);
  EXPECT_EQ(cass_vector_append_bytes(vector_, bytes2, sizeof(bytes2)), CASS_OK);
  EXPECT_EQ(cass_vector_append_bytes(vector_, bytes3, sizeof(bytes3)), CASS_OK);
}

TEST_F(VectorUnitTest, AppendUuid) {
  CassUuid uuid1 = {123456789, 987654321};
  CassUuid uuid2 = {111111111, 222222222};
  CassUuid uuid3 = {333333333, 444444444};
  
  EXPECT_EQ(cass_vector_append_uuid(vector_, uuid1), CASS_OK);
  EXPECT_EQ(cass_vector_append_uuid(vector_, uuid2), CASS_OK);
  EXPECT_EQ(cass_vector_append_uuid(vector_, uuid3), CASS_OK);
}

TEST_F(VectorUnitTest, AppendBool) {
  EXPECT_EQ(cass_vector_append_bool(vector_, cass_true), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(vector_, cass_false), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(vector_, cass_true), CASS_OK);
}

TEST_F(VectorUnitTest, AppendTuple) {
  CassTuple* tuple1 = cass_tuple_new(2);
  CassTuple* tuple2 = cass_tuple_new(2);
  CassTuple* tuple3 = cass_tuple_new(2);
  
  cass_tuple_set_int32(tuple1, 0, 1);
  cass_tuple_set_int32(tuple1, 1, 2);
  
  EXPECT_EQ(cass_vector_append_tuple(vector_, tuple1), CASS_OK);
  EXPECT_EQ(cass_vector_append_tuple(vector_, tuple2), CASS_OK);
  EXPECT_EQ(cass_vector_append_tuple(vector_, tuple3), CASS_OK);
  
  cass_tuple_free(tuple1);
  cass_tuple_free(tuple2);
  cass_tuple_free(tuple3);
}

TEST_F(VectorUnitTest, AppendCollection) {
  CassCollection* list1 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  CassCollection* list2 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  CassCollection* list3 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  
  cass_collection_append_int32(list1, 1);
  cass_collection_append_int32(list1, 2);
  
  EXPECT_EQ(cass_vector_append_collection(vector_, list1), CASS_OK);
  EXPECT_EQ(cass_vector_append_collection(vector_, list2), CASS_OK);
  EXPECT_EQ(cass_vector_append_collection(vector_, list3), CASS_OK);
  
  cass_collection_free(list1);
  cass_collection_free(list2);
  cass_collection_free(list3);
}

TEST_F(VectorUnitTest, AppendUserType) {
  // Create a mock user type (this would normally come from schema)
  CassUserType* udt1 = cass_user_type_new_from_data_type(NULL);
  CassUserType* udt2 = cass_user_type_new_from_data_type(NULL);
  CassUserType* udt3 = cass_user_type_new_from_data_type(NULL);
  
  if (udt1 && udt2 && udt3) {
    EXPECT_EQ(cass_vector_append_user_type(vector_, udt1), CASS_OK);
    EXPECT_EQ(cass_vector_append_user_type(vector_, udt2), CASS_OK);
    EXPECT_EQ(cass_vector_append_user_type(vector_, udt3), CASS_OK);
  }
  
  cass_user_type_free(udt1);
  cass_user_type_free(udt2);
  cass_user_type_free(udt3);
}

// Note: cass_vector_append_null doesn't exist - vectors have fixed types

TEST_F(VectorUnitTest, AppendInet) {
  CassInet inet1, inet2, inet3;
  ASSERT_EQ(cass_inet_from_string("127.0.0.1", &inet1), CASS_OK);
  ASSERT_EQ(cass_inet_from_string("192.168.1.1", &inet2), CASS_OK);
  ASSERT_EQ(cass_inet_from_string("::1", &inet3), CASS_OK);
  
  EXPECT_EQ(cass_vector_append_inet(vector_, inet1), CASS_OK);
  EXPECT_EQ(cass_vector_append_inet(vector_, inet2), CASS_OK);
  EXPECT_EQ(cass_vector_append_inet(vector_, inet3), CASS_OK);
}

TEST_F(VectorUnitTest, AppendDecimal) {
  const cass_byte_t varint1[] = {0x01, 0x02};
  const cass_byte_t varint2[] = {0x03, 0x04};
  const cass_byte_t varint3[] = {0x05, 0x06};
  
  EXPECT_EQ(cass_vector_append_decimal(vector_, varint1, sizeof(varint1), 2), CASS_OK);
  EXPECT_EQ(cass_vector_append_decimal(vector_, varint2, sizeof(varint2), 3), CASS_OK);
  EXPECT_EQ(cass_vector_append_decimal(vector_, varint3, sizeof(varint3), 4), CASS_OK);
}

TEST_F(VectorUnitTest, AppendDuration) {
  EXPECT_EQ(cass_vector_append_duration(vector_, 1, 2, 3), CASS_OK);
  EXPECT_EQ(cass_vector_append_duration(vector_, 4, 5, 6), CASS_OK);
  EXPECT_EQ(cass_vector_append_duration(vector_, 7, 8, 9), CASS_OK);
}

TEST_F(VectorUnitTest, AppendVector) {
  CassVector* nested1 = cass_vector_new(2);
  CassVector* nested2 = cass_vector_new(2);
  CassVector* nested3 = cass_vector_new(2);
  
  cass_vector_append_int32(nested1, 1);
  cass_vector_append_int32(nested1, 2);
  
  EXPECT_EQ(cass_vector_append_vector(vector_, nested1), CASS_OK);
  EXPECT_EQ(cass_vector_append_vector(vector_, nested2), CASS_OK);
  EXPECT_EQ(cass_vector_append_vector(vector_, nested3), CASS_OK);
  
  cass_vector_free(nested1);
  cass_vector_free(nested2);
  cass_vector_free(nested3);
}

TEST_F(VectorUnitTest, Serialization) {
  // Test float vector serialization
  cass_vector_append_float(vector_, 1.0f);
  cass_vector_append_float(vector_, 2.0f);
  cass_vector_append_float(vector_, 3.0f);
  
  VectorValue* vector_value = static_cast<VectorValue*>(vector_->from());
  Buffer encoded = vector_value->encode();
  
  // Verify encoding format
  EXPECT_GT(encoded.size(), static_cast<size_t>(0));
  
  // For floats, each is 4 bytes
  // Expected: 3 floats * 4 bytes = 12 bytes
  EXPECT_EQ(encoded.size(), static_cast<size_t>(12));
}

TEST_F(VectorUnitTest, SerializationWithLength) {
  cass_vector_append_int32(vector_, 100);
  cass_vector_append_int32(vector_, 200);
  cass_vector_append_int32(vector_, 300);
  
  VectorValue* vector_value = static_cast<VectorValue*>(vector_->from());
  Buffer encoded = vector_value->encode_with_length();
  
  // Should have 4-byte length prefix followed by data
  EXPECT_GT(encoded.size(), static_cast<size_t>(4));
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