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

#include "integration.hpp"

/**
 * Comprehensive test for vector append functions to ensure C API coverage.
 * This test validates that basic data types can be appended to vectors.
 */
class VectorAllAppendTypesTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Create test keyspace
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_append_test "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_append_test");
  }
};

/**
 * Test all basic numeric types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendNumericTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);

  session_.execute(
    "CREATE TABLE IF NOT EXISTS test_numeric_vectors ("
    "  id int PRIMARY KEY,"
    "  v_int vector<int, 2>,"
    "  v_bigint vector<bigint, 2>,"
    "  v_float vector<float, 2>,"
    "  v_double vector<double, 2>"
    ")");

  // Prepare insert statement
  const char* insert_query = 
    "INSERT INTO test_numeric_vectors (id, v_int, v_bigint, v_float, v_double) "
    "VALUES (?, ?, ?, ?, ?)";
  
  Prepared prepared = session_.prepare(insert_query);
  Statement statement = prepared.bind();
  
  // Bind id
  statement.bind<Integer>(0, Integer(1));
  
  // Test int32
  CassVector* v_int = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
  ASSERT_NE(v_int, nullptr);
  ASSERT_EQ(cass_vector_append_int32(v_int, 10), CASS_OK);
  ASSERT_EQ(cass_vector_append_int32(v_int, 20), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_int), CASS_OK);
  
  // Test int64 (bigint)
  CassVector* v_bigint = cass_vector_new(CASS_VALUE_TYPE_BIGINT, 2);
  ASSERT_NE(v_bigint, nullptr);
  ASSERT_EQ(cass_vector_append_int64(v_bigint, 10000000000LL), CASS_OK);
  ASSERT_EQ(cass_vector_append_int64(v_bigint, 20000000000LL), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_bigint), CASS_OK);
  
  // Test float
  CassVector* v_float = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 2);
  ASSERT_NE(v_float, nullptr);
  ASSERT_EQ(cass_vector_append_float(v_float, 1.5f), CASS_OK);
  ASSERT_EQ(cass_vector_append_float(v_float, 2.5f), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 3, v_float), CASS_OK);
  
  // Test double
  CassVector* v_double = cass_vector_new(CASS_VALUE_TYPE_DOUBLE, 2);
  ASSERT_NE(v_double, nullptr);
  ASSERT_EQ(cass_vector_append_double(v_double, 3.14159), CASS_OK);
  ASSERT_EQ(cass_vector_append_double(v_double, 2.71828), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 4, v_double), CASS_OK);
  
  // Execute
  Result result = session_.execute(statement, false);
  ASSERT_TRUE(result);
  
  // Clean up
  cass_vector_free(v_int);
  cass_vector_free(v_bigint);
  cass_vector_free(v_float);
  cass_vector_free(v_double);
  
  // Verify data was inserted
  result = session_.execute("SELECT * FROM test_numeric_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test string and bytes types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendStringBytesTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);

  session_.execute(
    "CREATE TABLE IF NOT EXISTS test_string_vectors ("
    "  id int PRIMARY KEY,"
    "  v_text vector<text, 2>,"
    "  v_blob vector<blob, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_string_vectors (id, v_text, v_blob) VALUES (?, ?, ?)";
  
  Prepared prepared = session_.prepare(insert_query);
  Statement statement = prepared.bind();
  
  statement.bind<Integer>(0, Integer(1));
  
  // Test text/string
  CassVector* v_text = cass_vector_new(CASS_VALUE_TYPE_TEXT, 2);
  ASSERT_NE(v_text, nullptr);
  ASSERT_EQ(cass_vector_append_string(v_text, "hello"), CASS_OK);
  ASSERT_EQ(cass_vector_append_string_n(v_text, "world", 5), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_text), CASS_OK);
  
  // Test blob/bytes
  CassVector* v_blob = cass_vector_new(CASS_VALUE_TYPE_BLOB, 2);
  ASSERT_NE(v_blob, nullptr);
  const cass_byte_t bytes1[] = {0xDE, 0xAD};
  const cass_byte_t bytes2[] = {0xBE, 0xEF};
  ASSERT_EQ(cass_vector_append_bytes(v_blob, bytes1, sizeof(bytes1)), CASS_OK);
  ASSERT_EQ(cass_vector_append_bytes(v_blob, bytes2, sizeof(bytes2)), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_blob), CASS_OK);
  
  // Execute
  Result result = session_.execute(statement, false);
  ASSERT_TRUE(result);
  
  cass_vector_free(v_text);
  cass_vector_free(v_blob);
  
  // Verify
  result = session_.execute("SELECT * FROM test_string_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test boolean and UUID types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendBooleanUuidTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);

  session_.execute(
    "CREATE TABLE IF NOT EXISTS test_bool_uuid_vectors ("
    "  id int PRIMARY KEY,"
    "  v_bool vector<boolean, 2>,"
    "  v_uuid vector<uuid, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_bool_uuid_vectors (id, v_bool, v_uuid) VALUES (?, ?, ?)";
  
  Prepared prepared = session_.prepare(insert_query);
  Statement statement = prepared.bind();
  
  statement.bind<Integer>(0, Integer(1));
  
  // Test boolean
  CassVector* v_bool = cass_vector_new(CASS_VALUE_TYPE_BOOLEAN, 2);
  ASSERT_NE(v_bool, nullptr);
  ASSERT_EQ(cass_vector_append_bool(v_bool, cass_true), CASS_OK);
  ASSERT_EQ(cass_vector_append_bool(v_bool, cass_false), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_bool), CASS_OK);
  
  // Test UUID
  CassVector* v_uuid = cass_vector_new(CASS_VALUE_TYPE_UUID, 2);
  ASSERT_NE(v_uuid, nullptr);
  CassUuidGen* uuid_gen = cass_uuid_gen_new();
  CassUuid uuid1;
  cass_uuid_gen_random(uuid_gen, &uuid1);
  CassUuid uuid2;
  cass_uuid_gen_random(uuid_gen, &uuid2);
  cass_uuid_gen_free(uuid_gen);
  ASSERT_EQ(cass_vector_append_uuid(v_uuid, uuid1), CASS_OK);
  ASSERT_EQ(cass_vector_append_uuid(v_uuid, uuid2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_uuid), CASS_OK);
  
  // Execute
  Result result = session_.execute(statement, false);
  ASSERT_TRUE(result);
  
  cass_vector_free(v_bool);
  cass_vector_free(v_uuid);
  
  // Verify
  result = session_.execute("SELECT * FROM test_bool_uuid_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test date and time types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendDateTimeTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);

  session_.execute(
    "CREATE TABLE IF NOT EXISTS test_datetime_vectors ("
    "  id int PRIMARY KEY,"
    "  v_date vector<date, 2>,"
    "  v_time vector<time, 2>,"
    "  v_timestamp vector<timestamp, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_datetime_vectors (id, v_date, v_time, v_timestamp) VALUES (?, ?, ?, ?)";
  
  Prepared prepared = session_.prepare(insert_query);
  Statement statement = prepared.bind();
  
  statement.bind<Integer>(0, Integer(1));
  
  // Test date
  CassVector* v_date = cass_vector_new(CASS_VALUE_TYPE_DATE, 2);
  ASSERT_NE(v_date, nullptr);
  ASSERT_EQ(cass_vector_append_date(v_date, 2147483648u), CASS_OK); // 1970-01-01
  ASSERT_EQ(cass_vector_append_date(v_date, 2147483649u), CASS_OK); // 1970-01-02
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_date), CASS_OK);
  
  // Test time
  CassVector* v_time = cass_vector_new(CASS_VALUE_TYPE_TIME, 2);
  ASSERT_NE(v_time, nullptr);
  ASSERT_EQ(cass_vector_append_time(v_time, 0), CASS_OK);
  ASSERT_EQ(cass_vector_append_time(v_time, 1000000000), CASS_OK); // 1 second
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_time), CASS_OK);
  
  // Test timestamp
  CassVector* v_timestamp = cass_vector_new(CASS_VALUE_TYPE_TIMESTAMP, 2);
  ASSERT_NE(v_timestamp, nullptr);
  ASSERT_EQ(cass_vector_append_timestamp(v_timestamp, 1234567890123LL), CASS_OK);
  ASSERT_EQ(cass_vector_append_timestamp(v_timestamp, 1234567890124LL), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 3, v_timestamp), CASS_OK);
  
  // Execute
  Result result = session_.execute(statement, false);
  ASSERT_TRUE(result);
  
  cass_vector_free(v_date);
  cass_vector_free(v_time);
  cass_vector_free(v_timestamp);
  
  // Verify
  result = session_.execute("SELECT * FROM test_datetime_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test decimal, duration and inet types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendComplexTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);

  session_.execute(
    "CREATE TABLE IF NOT EXISTS test_complex_vectors ("
    "  id int PRIMARY KEY,"
    "  v_decimal vector<decimal, 2>,"
    "  v_duration vector<duration, 2>,"
    "  v_inet vector<inet, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_complex_vectors (id, v_decimal, v_duration, v_inet) VALUES (?, ?, ?, ?)";
  
  Prepared prepared = session_.prepare(insert_query);
  Statement statement = prepared.bind();
  
  statement.bind<Integer>(0, Integer(1));
  
  // Test decimal
  CassVector* v_decimal = cass_vector_new(CASS_VALUE_TYPE_DECIMAL, 2);
  ASSERT_NE(v_decimal, nullptr);
  const cass_byte_t varint1[] = {0x01, 0x23};
  const cass_byte_t varint2[] = {0x45, 0x67};
  ASSERT_EQ(cass_vector_append_decimal(v_decimal, varint1, sizeof(varint1), 2), CASS_OK);
  ASSERT_EQ(cass_vector_append_decimal(v_decimal, varint2, sizeof(varint2), 3), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_decimal), CASS_OK);
  
  // Test duration
  CassVector* v_duration = cass_vector_new(CASS_VALUE_TYPE_DURATION, 2);
  ASSERT_NE(v_duration, nullptr);
  ASSERT_EQ(cass_vector_append_duration(v_duration, 1, 2, 3), CASS_OK);
  ASSERT_EQ(cass_vector_append_duration(v_duration, 4, 5, 6), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_duration), CASS_OK);
  
  // Test inet
  CassVector* v_inet = cass_vector_new(CASS_VALUE_TYPE_INET, 2);
  ASSERT_NE(v_inet, nullptr);
  CassInet inet1;
  cass_inet_from_string("127.0.0.1", &inet1);
  CassInet inet2;
  cass_inet_from_string("192.168.1.1", &inet2);
  ASSERT_EQ(cass_vector_append_inet(v_inet, inet1), CASS_OK);
  ASSERT_EQ(cass_vector_append_inet(v_inet, inet2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 3, v_inet), CASS_OK);
  
  // Execute
  Result result = session_.execute(statement, false);
  ASSERT_TRUE(result);
  
  cass_vector_free(v_decimal);
  cass_vector_free(v_duration);
  cass_vector_free(v_inet);
  
  // Verify
  result = session_.execute("SELECT * FROM test_complex_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test dimension overflow
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, DimensionOverflow) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);

  session_.execute(
    "CREATE TABLE IF NOT EXISTS test_overflow ("
    "  id int PRIMARY KEY,"
    "  vec vector<int, 2>"
    ")");

  CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
  ASSERT_NE(vec, nullptr);
  
  // Add two elements (fills the vector)
  ASSERT_EQ(cass_vector_append_int32(vec, 1), CASS_OK);
  ASSERT_EQ(cass_vector_append_int32(vec, 2), CASS_OK);
  
  // Try to add a third element - should fail with index out of bounds error
  CassError error = cass_vector_append_int32(vec, 3);
  ASSERT_EQ(error, CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  
  cass_vector_free(vec);
}

/**
 * Test null rejection
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, NullRejection) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);

  CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
  ASSERT_NE(vec, nullptr);
  
  // Vectors should reject null values
  CassError error = cass_vector_append_null(vec);
  ASSERT_EQ(error, CASS_ERROR_LIB_NULL_VALUE);
  
  cass_vector_free(vec);
}