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
#include "statement.hpp"
#include "cass_vector.hpp"
#include "external.hpp"
#include <cstring>

using namespace datastax::internal::core;

class VectorCAPITest : public ::testing::Test {
protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(VectorCAPITest, CreateVectorWithCAPI) {
  // Test creating a vector with the C API
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  ASSERT_NE(vector, nullptr);
  
  // Check dimension
  EXPECT_EQ(cass_vector_dimension(vector), 3);
  
  // Clean up
  cass_vector_free(vector);
}

TEST_F(VectorCAPITest, CreateVectorInvalidDimension) {
  // Test creating with invalid dimensions
  CassVector* vector1 = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 0);
  EXPECT_EQ(vector1, nullptr);
  
  CassVector* vector2 = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 8193);
  EXPECT_EQ(vector2, nullptr);
  
  // Valid boundary cases
  CassVector* vector3 = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 1);
  ASSERT_NE(vector3, nullptr);
  cass_vector_free(vector3);
  
  CassVector* vector4 = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 8192);
  ASSERT_NE(vector4, nullptr);
  cass_vector_free(vector4);
}

TEST_F(VectorCAPITest, AppendFloatsToVector) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  ASSERT_NE(vector, nullptr);
  
  // Append three floats
  EXPECT_EQ(cass_vector_append_float(vector, 1.0f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector, 2.0f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector, 3.0f), CASS_OK);
  
  // Fourth should fail (dimension exceeded)
  EXPECT_EQ(cass_vector_append_float(vector, 4.0f), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  
  cass_vector_free(vector);
}

TEST_F(VectorCAPITest, AppendDifferentTypes) {
  // Test int32 vector
  CassVector* int_vector = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
  ASSERT_NE(int_vector, nullptr);
  EXPECT_EQ(cass_vector_append_int32(int_vector, 42), CASS_OK);
  EXPECT_EQ(cass_vector_append_int32(int_vector, -17), CASS_OK);
  cass_vector_free(int_vector);
  
  // Test bigint vector
  CassVector* bigint_vector = cass_vector_new(CASS_VALUE_TYPE_BIGINT, 2);
  ASSERT_NE(bigint_vector, nullptr);
  EXPECT_EQ(cass_vector_append_int64(bigint_vector, 1000000000000LL), CASS_OK);
  EXPECT_EQ(cass_vector_append_int64(bigint_vector, -1000000000000LL), CASS_OK);
  cass_vector_free(bigint_vector);
  
  // Test text vector
  CassVector* text_vector = cass_vector_new(CASS_VALUE_TYPE_TEXT, 2);
  ASSERT_NE(text_vector, nullptr);
  EXPECT_EQ(cass_vector_append_string(text_vector, "hello"), CASS_OK);
  EXPECT_EQ(cass_vector_append_string_n(text_vector, "world", 5), CASS_OK);
  cass_vector_free(text_vector);
  
  // Test boolean vector
  CassVector* bool_vector = cass_vector_new(CASS_VALUE_TYPE_BOOLEAN, 3);
  ASSERT_NE(bool_vector, nullptr);
  EXPECT_EQ(cass_vector_append_bool(bool_vector, cass_true), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(bool_vector, cass_false), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(bool_vector, cass_true), CASS_OK);
  cass_vector_free(bool_vector);
}

TEST_F(VectorCAPITest, AppendUUID) {
  CassVector* uuid_vector = cass_vector_new(CASS_VALUE_TYPE_UUID, 2);
  ASSERT_NE(uuid_vector, nullptr);
  
  CassUuid uuid1;
  memset(&uuid1, 0, sizeof(uuid1));
  uuid1.time_and_version = 0x1234567890ABCDEF;
  uuid1.clock_seq_and_node = 0xFEDCBA0987654321;
  
  CassUuid uuid2;
  memset(&uuid2, 0xFF, sizeof(uuid2));
  
  EXPECT_EQ(cass_vector_append_uuid(uuid_vector, uuid1), CASS_OK);
  EXPECT_EQ(cass_vector_append_uuid(uuid_vector, uuid2), CASS_OK);
  
  cass_vector_free(uuid_vector);
}

TEST_F(VectorCAPITest, StatementBindVector) {
  // Create a statement
  CassStatement* statement = cass_statement_new("INSERT INTO test (id, vec) VALUES (?, ?)", 2);
  ASSERT_NE(statement, nullptr);
  
  // Create a vector
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  ASSERT_NE(vector, nullptr);
  
  // Add elements
  EXPECT_EQ(cass_vector_append_float(vector, 1.5f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector, 2.5f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector, 3.5f), CASS_OK);
  
  // Bind an int to first parameter
  EXPECT_EQ(cass_statement_bind_int32(statement, 0, 1), CASS_OK);
  
  // Bind vector to second parameter
  EXPECT_EQ(cass_statement_bind_vector(statement, 1, vector), CASS_OK);
  
  // Clean up
  cass_vector_free(vector);
  cass_statement_free(statement);
}

TEST_F(VectorCAPITest, StatementBindVectorByName) {
  // Create a statement with named parameters
  CassStatement* statement = cass_statement_new("INSERT INTO test (id, vec) VALUES (:id, :vec)", 2);
  ASSERT_NE(statement, nullptr);
  
  // Create a vector
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
  ASSERT_NE(vector, nullptr);
  
  EXPECT_EQ(cass_vector_append_int32(vector, 100), CASS_OK);
  EXPECT_EQ(cass_vector_append_int32(vector, 200), CASS_OK);
  
  // Bind by name
  EXPECT_EQ(cass_statement_bind_int32_by_name(statement, "id", 42), CASS_OK);
  EXPECT_EQ(cass_statement_bind_vector_by_name(statement, "vec", vector), CASS_OK);
  
  // Also test the _n variant
  EXPECT_EQ(cass_statement_bind_vector_by_name_n(statement, "vec", 3, vector), CASS_OK);
  
  cass_vector_free(vector);
  cass_statement_free(statement);
}

TEST_F(VectorCAPITest, VectorWithCollections) {
  // Test that cass_vector_new properly rejects collection types
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_LIST, 2);
  ASSERT_EQ(vector, nullptr) << "cass_vector_new should reject LIST type without inner type info";
  
  // Now test with the proper API
  // Create list<int> data type
  CassDataType* int_type = cass_data_type_new(CASS_VALUE_TYPE_INT);
  CassDataType* list_type = cass_data_type_new(CASS_VALUE_TYPE_LIST);
  cass_data_type_add_sub_type(list_type, int_type);
  cass_data_type_free(int_type);
  
  // Create vector with proper element type
  vector = cass_vector_new_with_element_type(list_type, 2);
  ASSERT_NE(vector, nullptr) << "cass_vector_new_with_element_type should work with list<int>";
  
  // Create first list
  CassCollection* list1 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  ASSERT_NE(list1, nullptr);
  EXPECT_EQ(cass_collection_append_int32(list1, 1), CASS_OK);
  EXPECT_EQ(cass_collection_append_int32(list1, 2), CASS_OK);
  
  // Create second list
  CassCollection* list2 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 3);
  ASSERT_NE(list2, nullptr);
  EXPECT_EQ(cass_collection_append_int32(list2, 3), CASS_OK);
  EXPECT_EQ(cass_collection_append_int32(list2, 4), CASS_OK);
  EXPECT_EQ(cass_collection_append_int32(list2, 5), CASS_OK);
  
  // Add lists to vector
  EXPECT_EQ(cass_vector_append_collection(vector, list1), CASS_OK);
  EXPECT_EQ(cass_vector_append_collection(vector, list2), CASS_OK);
  
  // Clean up
  cass_collection_free(list1);
  cass_collection_free(list2);
  cass_vector_free(vector);
  cass_data_type_free(list_type);
}

TEST_F(VectorCAPITest, VectorGetDataType) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_DOUBLE, 4);
  ASSERT_NE(vector, nullptr);
  
  const CassDataType* data_type = cass_vector_data_type(vector);
  ASSERT_NE(data_type, nullptr);
  
  // The data type should be a custom type (vectors are custom types)
  EXPECT_EQ(cass_data_type_type(data_type), CASS_VALUE_TYPE_CUSTOM);
  
  cass_vector_free(vector);
}

TEST_F(VectorCAPITest, AppendBytesAndCustom) {
  // Test blob vector
  CassVector* blob_vector = cass_vector_new(CASS_VALUE_TYPE_BLOB, 2);
  ASSERT_NE(blob_vector, nullptr);
  
  const cass_byte_t bytes1[] = {0xDE, 0xAD, 0xBE, 0xEF};
  const cass_byte_t bytes2[] = {0xCA, 0xFE, 0xBA, 0xBE};
  
  EXPECT_EQ(cass_vector_append_bytes(blob_vector, bytes1, sizeof(bytes1)), CASS_OK);
  EXPECT_EQ(cass_vector_append_bytes(blob_vector, bytes2, sizeof(bytes2)), CASS_OK);
  
  cass_vector_free(blob_vector);
  
  // Test that custom type is rejected by cass_vector_new
  CassVector* custom_vector = cass_vector_new(CASS_VALUE_TYPE_CUSTOM, 1);
  ASSERT_EQ(custom_vector, nullptr) << "cass_vector_new should reject CUSTOM type without class name";
  
  // Note: Creating a vector with CUSTOM element type would require 
  // using cass_vector_new_from_data_type with a proper custom DataType
  // which is complex to set up in a unit test
}

TEST_F(VectorCAPITest, AppendDecimalAndDuration) {
  // Test decimal vector
  CassVector* decimal_vector = cass_vector_new(CASS_VALUE_TYPE_DECIMAL, 2);
  ASSERT_NE(decimal_vector, nullptr);
  
  const cass_byte_t varint1[] = {0x01, 0x00};
  const cass_byte_t varint2[] = {0xFF, 0xFF};
  
  EXPECT_EQ(cass_vector_append_decimal(decimal_vector, varint1, sizeof(varint1), 2), CASS_OK);
  EXPECT_EQ(cass_vector_append_decimal(decimal_vector, varint2, sizeof(varint2), -3), CASS_OK);
  
  cass_vector_free(decimal_vector);
  
  // Test duration vector
  CassVector* duration_vector = cass_vector_new(CASS_VALUE_TYPE_DURATION, 2);
  ASSERT_NE(duration_vector, nullptr);
  
  EXPECT_EQ(cass_vector_append_duration(duration_vector, 1, 2, 3000000000LL), CASS_OK);
  EXPECT_EQ(cass_vector_append_duration(duration_vector, -1, -2, -3000000000LL), CASS_OK);
  
  cass_vector_free(duration_vector);
}

TEST_F(VectorCAPITest, AppendInet) {
  CassVector* inet_vector = cass_vector_new(CASS_VALUE_TYPE_INET, 2);
  ASSERT_NE(inet_vector, nullptr);
  
  CassInet inet4;
  inet4.address_length = 4;
  memcpy(inet4.address, "\x7F\x00\x00\x01", 4); // 127.0.0.1
  
  CassInet inet6;
  inet6.address_length = 16;
  memset(inet6.address, 0, 16);
  inet6.address[15] = 1; // ::1
  
  EXPECT_EQ(cass_vector_append_inet(inet_vector, inet4), CASS_OK);
  EXPECT_EQ(cass_vector_append_inet(inet_vector, inet6), CASS_OK);
  
  cass_vector_free(inet_vector);
}

// Test for internal encoding verification
TEST_F(VectorCAPITest, VerifyFloatVectorEncoding) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  ASSERT_NE(vector, nullptr);
  
  // Add known float values
  EXPECT_EQ(cass_vector_append_float(vector, 1.0f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector, 2.0f), CASS_OK);
  EXPECT_EQ(cass_vector_append_float(vector, 3.0f), CASS_OK);
  
  // Get the internal vector to check encoding
  CassandraVector* internal_vector = vector->from();
  Buffer encoded = internal_vector->encode();
  
  // Expected encoding (big-endian floats concatenated):
  // 1.0f = 0x3F800000
  // 2.0f = 0x40000000  
  // 3.0f = 0x40400000
  const unsigned char expected[] = {
    0x3F, 0x80, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x00,
    0x40, 0x40, 0x00, 0x00
  };
  
  EXPECT_EQ(encoded.size(), sizeof(expected));
  EXPECT_EQ(memcmp(encoded.data(), expected, sizeof(expected)), 0);
  
  cass_vector_free(vector);
}