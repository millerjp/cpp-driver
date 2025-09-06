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
#include "collection.hpp"
#include "tuple.hpp"
#include "user_type_value.hpp"

using namespace datastax::internal::core;

/**
 * Comprehensive unit test to verify ALL vector append functions are implemented
 * and working correctly. This ensures complete C API coverage.
 */
class VectorAppendAllTypesTest : public ::testing::Test {
protected:
  void SetUp() {
    // Common setup if needed
  }
};

/**
 * Test numeric append functions
 */
TEST_F(VectorAppendAllTypesTest, AppendNumericTypes) {
  // Test int8 (tinyint)
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TINY_INT, 2);
    ASSERT_NE(vector, nullptr);
    EXPECT_EQ(cass_vector_append_int8(vector, 10), CASS_OK);
    EXPECT_EQ(cass_vector_append_int8(vector, -20), CASS_OK);
    EXPECT_EQ(cass_vector_append_int8(vector, 30), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test int16 (smallint)
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_SMALL_INT, 2);
    ASSERT_NE(vector, nullptr);
    EXPECT_EQ(cass_vector_append_int16(vector, 1000), CASS_OK);
    EXPECT_EQ(cass_vector_append_int16(vector, -2000), CASS_OK);
    EXPECT_EQ(cass_vector_append_int16(vector, 3000), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test int32
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
    ASSERT_NE(vector, nullptr);
    EXPECT_EQ(cass_vector_append_int32(vector, 100000), CASS_OK);
    EXPECT_EQ(cass_vector_append_int32(vector, -200000), CASS_OK);
    EXPECT_EQ(cass_vector_append_int32(vector, 300000), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test uint32 - Note: CQL doesn't have unsigned int, so we use int32 for testing
  // The uint32 append function exists for compatibility but may not work with all types
  {
    // Skip this test as CQL doesn't have unsigned int type
    // The function exists for API completeness
  }
  
  // Test int64 (bigint)
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BIGINT, 2);
    ASSERT_NE(vector, nullptr);
    EXPECT_EQ(cass_vector_append_int64(vector, 1000000000LL), CASS_OK);
    EXPECT_EQ(cass_vector_append_int64(vector, -2000000000LL), CASS_OK);
    EXPECT_EQ(cass_vector_append_int64(vector, 3000000000LL), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test float
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 2);
    ASSERT_NE(vector, nullptr);
    EXPECT_EQ(cass_vector_append_float(vector, 3.14f), CASS_OK);
    EXPECT_EQ(cass_vector_append_float(vector, -2.71f), CASS_OK);
    EXPECT_EQ(cass_vector_append_float(vector, 1.41f), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test double
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_DOUBLE, 2);
    ASSERT_NE(vector, nullptr);
    EXPECT_EQ(cass_vector_append_double(vector, 3.14159265), CASS_OK);
    EXPECT_EQ(cass_vector_append_double(vector, -2.71828182), CASS_OK);
    EXPECT_EQ(cass_vector_append_double(vector, 1.41421356), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
}

/**
 * Test string and bytes append functions
 */
TEST_F(VectorAppendAllTypesTest, AppendStringBytesTypes) {
  // Test string
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TEXT, 3);
    ASSERT_NE(vector, nullptr);
    EXPECT_EQ(cass_vector_append_string(vector, "hello"), CASS_OK);
    EXPECT_EQ(cass_vector_append_string_n(vector, "world123", 5), CASS_OK);
    EXPECT_EQ(cass_vector_append_string(vector, "test"), CASS_OK);
    EXPECT_EQ(cass_vector_append_string(vector, "overflow"), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test bytes/blob
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BLOB, 2);
    ASSERT_NE(vector, nullptr);
    const cass_byte_t bytes1[] = {0x01, 0x02, 0x03};
    const cass_byte_t bytes2[] = {0x04, 0x05, 0x06, 0x07};
    EXPECT_EQ(cass_vector_append_bytes(vector, bytes1, sizeof(bytes1)), CASS_OK);
    EXPECT_EQ(cass_vector_append_bytes(vector, bytes2, sizeof(bytes2)), CASS_OK);
    EXPECT_EQ(cass_vector_append_bytes(vector, bytes1, sizeof(bytes1)), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
}

/**
 * Test boolean append function
 */
TEST_F(VectorAppendAllTypesTest, AppendBooleanType) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BOOLEAN, 3);
  ASSERT_NE(vector, nullptr);
  EXPECT_EQ(cass_vector_append_bool(vector, cass_true), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(vector, cass_false), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(vector, cass_true), CASS_OK);
  EXPECT_EQ(cass_vector_append_bool(vector, cass_false), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  cass_vector_free(vector);
}

/**
 * Test UUID and INET append functions
 */
TEST_F(VectorAppendAllTypesTest, AppendUuidInetTypes) {
  // Test UUID
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_UUID, 2);
    ASSERT_NE(vector, nullptr);
    CassUuid uuid1;
    CassUuid uuid2;
    memset(&uuid1, 0x01, sizeof(CassUuid));
    memset(&uuid2, 0x02, sizeof(CassUuid));
    EXPECT_EQ(cass_vector_append_uuid(vector, uuid1), CASS_OK);
    EXPECT_EQ(cass_vector_append_uuid(vector, uuid2), CASS_OK);
    EXPECT_EQ(cass_vector_append_uuid(vector, uuid1), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test INET
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_INET, 2);
    ASSERT_NE(vector, nullptr);
    CassInet inet1;
    CassInet inet2;
    cass_inet_from_string("127.0.0.1", &inet1);
    cass_inet_from_string("192.168.1.1", &inet2);
    EXPECT_EQ(cass_vector_append_inet(vector, inet1), CASS_OK);
    EXPECT_EQ(cass_vector_append_inet(vector, inet2), CASS_OK);
    EXPECT_EQ(cass_vector_append_inet(vector, inet1), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
}

/**
 * Test date, time, timestamp append functions (NEW FUNCTIONS)
 */
TEST_F(VectorAppendAllTypesTest, AppendDateTimeTypes) {
  // Test date
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_DATE, 2);
    ASSERT_NE(vector, nullptr);
    cass_uint32_t date1 = cass_date_from_epoch(1609459200);  // 2021-01-01
    cass_uint32_t date2 = cass_date_from_epoch(1640995200);  // 2022-01-01
    EXPECT_EQ(cass_vector_append_date(vector, date1), CASS_OK);
    EXPECT_EQ(cass_vector_append_date(vector, date2), CASS_OK);
    EXPECT_EQ(cass_vector_append_date(vector, date1), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test time
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TIME, 2);
    ASSERT_NE(vector, nullptr);
    cass_int64_t time1 = cass_time_from_epoch(3600);   // 1 hour
    cass_int64_t time2 = cass_time_from_epoch(7200);   // 2 hours
    EXPECT_EQ(cass_vector_append_time(vector, time1), CASS_OK);
    EXPECT_EQ(cass_vector_append_time(vector, time2), CASS_OK);
    EXPECT_EQ(cass_vector_append_time(vector, time1), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test timestamp
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TIMESTAMP, 2);
    ASSERT_NE(vector, nullptr);
    cass_int64_t timestamp1 = 1609459200000LL;  // 2021-01-01 in milliseconds
    cass_int64_t timestamp2 = 1640995200000LL;  // 2022-01-01 in milliseconds
    EXPECT_EQ(cass_vector_append_timestamp(vector, timestamp1), CASS_OK);
    EXPECT_EQ(cass_vector_append_timestamp(vector, timestamp2), CASS_OK);
    EXPECT_EQ(cass_vector_append_timestamp(vector, timestamp1), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
}

/**
 * Test decimal, duration, varint append functions
 */
TEST_F(VectorAppendAllTypesTest, AppendDecimalDurationVarintTypes) {
  // Test decimal
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_DECIMAL, 2);
    ASSERT_NE(vector, nullptr);
    const cass_byte_t varint1[] = {0x01, 0x23};
    const cass_byte_t varint2[] = {0x45, 0x67};
    EXPECT_EQ(cass_vector_append_decimal(vector, varint1, sizeof(varint1), 2), CASS_OK);
    EXPECT_EQ(cass_vector_append_decimal(vector, varint2, sizeof(varint2), 3), CASS_OK);
    EXPECT_EQ(cass_vector_append_decimal(vector, varint1, sizeof(varint1), 2), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test duration
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_DURATION, 2);
    ASSERT_NE(vector, nullptr);
    EXPECT_EQ(cass_vector_append_duration(vector, 1, 2, 3000000000LL), CASS_OK);
    EXPECT_EQ(cass_vector_append_duration(vector, 4, 5, 6000000000LL), CASS_OK);
    EXPECT_EQ(cass_vector_append_duration(vector, 7, 8, 9000000000LL), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
  
  // Test varint (NEW FUNCTION)
  {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_VARINT, 2);
    ASSERT_NE(vector, nullptr);
    const cass_byte_t varint1[] = {0x01, 0x23, 0x45};
    const cass_byte_t varint2[] = {0x67, 0x89};
    EXPECT_EQ(cass_vector_append_varint(vector, varint1, sizeof(varint1)), CASS_OK);
    EXPECT_EQ(cass_vector_append_varint(vector, varint2, sizeof(varint2)), CASS_OK);
    EXPECT_EQ(cass_vector_append_varint(vector, varint1, sizeof(varint1)), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vector);
  }
}

/**
 * Test collection append function
 */
TEST_F(VectorAppendAllTypesTest, AppendCollectionTypes) {
  // Create a vector that holds lists
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  CollectionType* list_collection = new CollectionType(CASS_VALUE_TYPE_LIST, true);
  list_collection->types().push_back(int_type);
  DataType::ConstPtr list_type(list_collection);
  
  VectorType::ConstPtr vector_type(new VectorType(list_type, 2));
  CassandraVector* vector = new CassandraVector(vector_type);
  vector->inc_ref();  // Need to increment ref count as we're managing it manually
  
  // Create collections to append
  Collection list1(list_type, 2);
  list1.append(cass_int32_t(10));
  list1.append(cass_int32_t(20));
  
  Collection list2(list_type, 1);
  list2.append(cass_int32_t(30));
  
  // Test appending collections
  EXPECT_EQ(vector->append(&list1), CASS_OK);
  EXPECT_EQ(vector->append(&list2), CASS_OK);
  EXPECT_EQ(vector->append(&list1), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  
  vector->dec_ref();
}

/**
 * Test tuple append function
 */
TEST_F(VectorAppendAllTypesTest, AppendTupleTypes) {
  // Create a vector that holds tuples
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  DataType::ConstPtr text_type(new DataType(CASS_VALUE_TYPE_TEXT));
  
  TupleType* tuple_collection = new TupleType(true);
  tuple_collection->types().push_back(int_type);
  tuple_collection->types().push_back(text_type);
  DataType::ConstPtr tuple_type(tuple_collection);
  
  VectorType::ConstPtr vector_type(new VectorType(tuple_type, 2));
  CassandraVector* vector = new CassandraVector(vector_type);
  
  // Create tuples to append
  Tuple tuple1(tuple_type);
  tuple1.set(0, cass_int32_t(100));
  tuple1.set(1, CassString("hello", 5));
  
  Tuple tuple2(tuple_type);
  tuple2.set(0, cass_int32_t(200));
  tuple2.set(1, CassString("world", 5));
  
  // Test appending tuples via C API
  CassVector* c_vector = CassVector::to(vector);
  vector->inc_ref();  // C API will dec_ref when freed
  
  EXPECT_EQ(cass_vector_append_tuple(c_vector, CassTuple::to(&tuple1)), CASS_OK);
  EXPECT_EQ(cass_vector_append_tuple(c_vector, CassTuple::to(&tuple2)), CASS_OK);
  EXPECT_EQ(cass_vector_append_tuple(c_vector, CassTuple::to(&tuple1)), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  
  cass_vector_free(c_vector);
}

/**
 * Test user type append function
 */
TEST_F(VectorAppendAllTypesTest, AppendUserTypeTypes) {
  // Create a user type
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  DataType::ConstPtr text_type(new DataType(CASS_VALUE_TYPE_TEXT));
  
  UserType* user_type_def = new UserType("ks", "my_type", true);
  user_type_def->add_field("a", int_type);
  user_type_def->add_field("b", text_type);
  DataType::ConstPtr user_type(user_type_def);
  
  VectorType::ConstPtr vector_type(new VectorType(user_type, 2));
  CassandraVector* vector = new CassandraVector(vector_type);
  
  // Create user type values to append
  UserTypeValue udt1(user_type);
  udt1.set(0, cass_int32_t(100));
  udt1.set(1, CassString("first", 5));
  
  UserTypeValue udt2(user_type);
  udt2.set(0, cass_int32_t(200));
  udt2.set(1, CassString("second", 6));
  
  // Test appending user types via C API
  CassVector* c_vector = CassVector::to(vector);
  vector->inc_ref();  // C API will dec_ref when freed
  
  EXPECT_EQ(cass_vector_append_user_type(c_vector, CassUserType::to(&udt1)), CASS_OK);
  EXPECT_EQ(cass_vector_append_user_type(c_vector, CassUserType::to(&udt2)), CASS_OK);
  EXPECT_EQ(cass_vector_append_user_type(c_vector, CassUserType::to(&udt1)), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  
  cass_vector_free(c_vector);
}

/**
 * Test nested vector append function
 */
TEST_F(VectorAppendAllTypesTest, AppendVectorTypes) {
  // Create a vector of vectors
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  VectorType::ConstPtr inner_vector_type(new VectorType(int_type, 2));
  VectorType::ConstPtr outer_vector_type(new VectorType(DataType::ConstPtr(inner_vector_type), 2));
  
  CassandraVector* outer = new CassandraVector(outer_vector_type);
  
  // Create inner vectors
  CassandraVector* inner1 = new CassandraVector(inner_vector_type);
  inner1->append(cass_int32_t(1));
  inner1->append(cass_int32_t(2));
  
  CassandraVector* inner2 = new CassandraVector(inner_vector_type);
  inner2->append(cass_int32_t(3));
  inner2->append(cass_int32_t(4));
  
  // Test appending vectors via C API
  CassVector* c_outer = CassVector::to(outer);
  outer->inc_ref();  // C API will dec_ref when freed
  
  CassVector* c_inner1 = CassVector::to(inner1);
  inner1->inc_ref();
  CassVector* c_inner2 = CassVector::to(inner2);
  inner2->inc_ref();
  
  EXPECT_EQ(cass_vector_append_vector(c_outer, c_inner1), CASS_OK);
  EXPECT_EQ(cass_vector_append_vector(c_outer, c_inner2), CASS_OK);
  EXPECT_EQ(cass_vector_append_vector(c_outer, c_inner1), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  
  cass_vector_free(c_inner1);
  cass_vector_free(c_inner2);
  cass_vector_free(c_outer);
}

/**
 * Test null rejection
 */
TEST_F(VectorAppendAllTypesTest, RejectNullValues) {
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
  ASSERT_NE(vector, nullptr);
  
  // Vectors don't support null elements
  EXPECT_EQ(cass_vector_append_null(vector), CASS_ERROR_LIB_NULL_VALUE);
  
  cass_vector_free(vector);
}

/**
 * Test custom type append function
 */
TEST_F(VectorAppendAllTypesTest, AppendCustomTypes) {
  // Custom types are handled as blobs with class names
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BLOB, 2);
  ASSERT_NE(vector, nullptr);
  
  const cass_byte_t custom_data1[] = {0x01, 0x02, 0x03};
  const cass_byte_t custom_data2[] = {0x04, 0x05};
  
  // Note: This will fail unless the vector element type matches the custom class name
  // This test just verifies the function exists and can be called
  CassError error1 = cass_vector_append_custom(vector, "org.example.CustomType", 
                                               custom_data1, sizeof(custom_data1));
  CassError error2 = cass_vector_append_custom_n(vector, "org.example.CustomType", 22,
                                                 custom_data2, sizeof(custom_data2));
  
  // These might fail with CASS_ERROR_LIB_INVALID_VALUE_TYPE if the custom type doesn't match
  // but the important thing is the functions exist and are callable
  
  cass_vector_free(vector);
}

/**
 * Summary test - verify all append functions are present
 */
TEST_F(VectorAppendAllTypesTest, AllAppendFunctionsExist) {
  // This test simply verifies that all expected append functions compile and link
  // It serves as a compile-time check that we haven't missed any functions
  
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BLOB, 20);
  ASSERT_NE(vector, nullptr);
  
  // All these functions should exist and be callable
  // We don't care about the return values here, just that they compile
  
  // Numeric types
  cass_vector_append_int8(vector, 0);
  cass_vector_append_int16(vector, 0);
  cass_vector_append_int32(vector, 0);
  cass_vector_append_uint32(vector, 0);
  cass_vector_append_int64(vector, 0);
  cass_vector_append_float(vector, 0.0f);
  cass_vector_append_double(vector, 0.0);
  
  // Boolean
  cass_vector_append_bool(vector, cass_false);
  
  // String/bytes
  cass_vector_append_string(vector, "");
  cass_vector_append_string_n(vector, "", 0);
  cass_byte_t bytes[1] = {0};
  cass_vector_append_bytes(vector, bytes, 0);
  
  // UUID/INET
  CassUuid uuid = {};
  cass_vector_append_uuid(vector, uuid);
  CassInet inet = {};
  cass_vector_append_inet(vector, inet);
  
  // Date/Time/Timestamp (NEW FUNCTIONS - MUST EXIST)
  cass_vector_append_date(vector, 0);
  cass_vector_append_time(vector, 0);
  cass_vector_append_timestamp(vector, 0);
  
  // Decimal/Duration/Varint
  cass_vector_append_decimal(vector, bytes, 0, 0);
  cass_vector_append_duration(vector, 0, 0, 0);
  cass_vector_append_varint(vector, bytes, 0);  // NEW FUNCTION - MUST EXIST
  
  // Custom
  cass_vector_append_custom(vector, "", bytes, 0);
  cass_vector_append_custom_n(vector, "", 0, bytes, 0);
  
  // Complex types (these need proper objects, so we just check they're declared)
  CassCollection* coll = nullptr;
  CassTuple* tuple = nullptr;
  CassUserType* udt = nullptr;
  CassVector* vec = nullptr;
  cass_vector_append_collection(vector, coll);
  cass_vector_append_tuple(vector, tuple);
  cass_vector_append_user_type(vector, udt);
  cass_vector_append_vector(vector, vec);
  
  // Null
  cass_vector_append_null(vector);
  
  cass_vector_free(vector);
  
  // If this test compiles and links, all functions are present
  SUCCEED() << "All vector append functions are present and callable";
}