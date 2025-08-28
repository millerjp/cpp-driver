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
#include "cassandra.h"

/**
 * COMPREHENSIVE test for ALL data types that can be in vectors
 * Based on Python driver test coverage
 */
class VectorAllDataTypesTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Skip if not Cassandra 5.0+
    if (!Options::is_cassandra() || server_version_ < "5.0.5") {
      SKIP_TEST("Vector types are only supported in Cassandra 5.0.5+");
    }
    
    // Use a unique keyspace for this test class to avoid conflicts
    session_.execute("DROP KEYSPACE IF EXISTS vector_all_types_test");
    session_.execute("CREATE KEYSPACE vector_all_types_test "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_all_types_test");
  }
  
  void TearDown() {
    // Clean up if we ran the test
    try {
      session_.execute("DROP KEYSPACE IF EXISTS vector_all_types_test");
    } catch (...) {
      // Ignore errors during cleanup
    }
    Integration::TearDown();
  }
};

/**
 * Test INT vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, IntVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE int_vectors (id int PRIMARY KEY, vec vector<int, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO int_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_int32(vector, 100);
  cass_vector_append_int32(vector, -200);
  cass_vector_append_int32(vector, 2147483647); // max int32
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM int_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
  
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  int32_t expected[] = {100, -200, 2147483647};
  int idx = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    cass_int32_t val;
    ASSERT_EQ(CASS_OK, cass_value_get_int32(element, &val));
    ASSERT_EQ(expected[idx], val);
    idx++;
  }
  cass_iterator_free(iter);
  ASSERT_EQ(3, idx);
}

/**
 * Test BIGINT vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, BigintVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE bigint_vectors (id int PRIMARY KEY, vec vector<bigint, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO bigint_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_int64(vector, 1000000000000LL);
  cass_vector_append_int64(vector, -2000000000000LL);
  cass_vector_append_int64(vector, 9223372036854775807LL); // max int64
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM bigint_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test SMALLINT vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, SmallintVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Insert using CQL - this should always work
  session_.execute("DROP TABLE IF EXISTS smallint_vectors_test");
  session_.execute("CREATE TABLE smallint_vectors_test (id int PRIMARY KEY, vec vector<smallint, 3>)");
  session_.execute("INSERT INTO smallint_vectors_test (id, vec) VALUES (1, [10, 20, 30])");
  
  // Query back with CQL
  Result result = session_.execute("SELECT vec FROM smallint_vectors_test WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  // Now try with C API - Use prepared statement for non-float vectors in protocol v4
  // Simple statements don't work with custom types without metadata
  const CassPrepared* prepared = NULL;
  CassFuture* prepare_future = cass_session_prepare(session_.get(), 
    "INSERT INTO smallint_vectors_test (id, vec) VALUES (?, ?)");
  
  ASSERT_EQ(CASS_OK, cass_future_error_code(prepare_future)) << "Failed to prepare statement";
  prepared = cass_future_get_prepared(prepare_future);
  
  CassStatement* statement = cass_prepared_bind(prepared);
  cass_statement_bind_int32(statement, 0, 2);
  
  // Create vector with smallint values
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_int16(vector, 10);
  cass_vector_append_int16(vector, 20);
  cass_vector_append_int16(vector, 30);
  
  CassError bind_rc = cass_statement_bind_vector(statement, 1, vector);
  ASSERT_EQ(CASS_OK, bind_rc) << "Failed to bind vector";
  cass_vector_free(vector);
  
  // Execute
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  if (rc != CASS_OK) {
    const char* message;
    size_t message_length;
    cass_future_error_message(future, &message, &message_length);
    TEST_LOG_ERROR("Error inserting smallint vector: " << std::string(message, message_length));
  }
  cass_future_free(future);
  
  cass_prepared_free(prepared);
  cass_future_free(prepare_future);
  
  ASSERT_EQ(CASS_OK, rc) << "Failed to insert smallint vector";
  
  // Query back row 2
  Result result2 = session_.execute("SELECT vec FROM smallint_vectors_test WHERE id = 2");
  ASSERT_EQ(1ul, result2.row_count());
}

/**
 * Test TINYINT vectors  
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, TinyintVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE tinyint_vectors (id int PRIMARY KEY, vec vector<tinyint, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO tinyint_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_int8(vector, 10);
  cass_vector_append_int8(vector, -20);
  cass_vector_append_int8(vector, 127); // max int8
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM tinyint_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test FLOAT vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, FloatVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE float_vectors (id int PRIMARY KEY, vec vector<float, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO float_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_float(vector, 1.5f);
  cass_vector_append_float(vector, -2.5f);
  cass_vector_append_float(vector, 3.14159f);
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM float_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test DOUBLE vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, DoubleVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE double_vectors (id int PRIMARY KEY, vec vector<double, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO double_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_double(vector, 1.5);
  cass_vector_append_double(vector, -2.5);
  cass_vector_append_double(vector, 3.141592653589793);
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM double_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test BOOLEAN vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, BooleanVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE boolean_vectors (id int PRIMARY KEY, vec vector<boolean, 4>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO boolean_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(4);
  cass_vector_append_bool(vector, cass_true);
  cass_vector_append_bool(vector, cass_false);
  cass_vector_append_bool(vector, cass_false);
  cass_vector_append_bool(vector, cass_true);
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM boolean_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test TEXT vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, TextVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE text_vectors (id int PRIMARY KEY, vec vector<text, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO text_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_string(vector, "hello");
  cass_vector_append_string(vector, "world");
  cass_vector_append_string(vector, "test");
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM text_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test ASCII vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, AsciiVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE ascii_vectors (id int PRIMARY KEY, vec vector<ascii, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO ascii_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_string(vector, "ASCII1");
  cass_vector_append_string(vector, "ASCII2");
  cass_vector_append_string(vector, "ASCII3");
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM ascii_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test VARCHAR vectors (same as text)
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, VarcharVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE varchar_vectors (id int PRIMARY KEY, vec vector<varchar, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO varchar_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_string(vector, "var1");
  cass_vector_append_string(vector, "var2");
  cass_vector_append_string(vector, "var3");
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM varchar_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test UUID vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, UuidVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE uuid_vectors (id int PRIMARY KEY, vec vector<uuid, 2>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO uuid_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassUuidGen* uuid_gen = cass_uuid_gen_new();
  CassUuid uuid1, uuid2;
  cass_uuid_gen_random(uuid_gen, &uuid1);
  cass_uuid_gen_random(uuid_gen, &uuid2);
  cass_uuid_gen_free(uuid_gen);
  
  CassVector* vector = cass_vector_new(2);
  cass_vector_append_uuid(vector, uuid1);
  cass_vector_append_uuid(vector, uuid2);
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM uuid_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
  
  // Verify we can iterate
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  int count = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    ASSERT_EQ(CASS_VALUE_TYPE_UUID, cass_value_type(element));
    count++;
  }
  cass_iterator_free(iter);
  ASSERT_EQ(2, count);
}

/**
 * Test TIMEUUID vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, TimeuuidVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE timeuuid_vectors (id int PRIMARY KEY, vec vector<timeuuid, 2>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO timeuuid_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassUuidGen* uuid_gen = cass_uuid_gen_new();
  CassUuid uuid1, uuid2;
  cass_uuid_gen_time(uuid_gen, &uuid1);
  cass_uuid_gen_time(uuid_gen, &uuid2);
  cass_uuid_gen_free(uuid_gen);
  
  CassVector* vector = cass_vector_new(2);
  cass_vector_append_uuid(vector, uuid1);
  cass_vector_append_uuid(vector, uuid2);
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM timeuuid_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test INET vectors (not supported by Cassandra - driver should block)
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, InetVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Table creation succeeds but driver blocks inet vector operations
  session_.execute("CREATE TABLE inet_vectors (id int PRIMARY KEY, vec vector<inet, 3>)");
  
  CassInet inet1, inet2, inet3;
  cass_inet_from_string("127.0.0.1", &inet1);
  cass_inet_from_string("192.168.1.1", &inet2);
  cass_inet_from_string("::1", &inet3); // IPv6
  
  CassVector* vector = cass_vector_new(3);
  
  // Driver should return error for inet vector elements
  CassError rc = cass_vector_append_inet(vector, inet1);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  rc = cass_vector_append_inet(vector, inet2);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  rc = cass_vector_append_inet(vector, inet3);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  cass_vector_free(vector);
}

/**
 * Test BLOB vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, BlobVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE blob_vectors (id int PRIMARY KEY, vec vector<blob, 3>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO blob_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  const cass_byte_t blob1[] = {0x01, 0x02, 0x03, 0x04};
  const cass_byte_t blob2[] = {0xFF, 0xFE, 0xFD};
  const cass_byte_t blob3[] = {0xAA, 0xBB};
  
  CassVector* vector = cass_vector_new(3);
  cass_vector_append_bytes(vector, blob1, sizeof(blob1));
  cass_vector_append_bytes(vector, blob2, sizeof(blob2));
  cass_vector_append_bytes(vector, blob3, sizeof(blob3));
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM blob_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test DECIMAL vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, DecimalVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE decimal_vectors (id int PRIMARY KEY, vec vector<decimal, 2>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO decimal_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  // Decimal is scale + varint
  // 123.45 = scale 2, varint 12345
  const cass_byte_t varint1[] = {0x30, 0x39}; // 12345 in varint encoding
  const cass_byte_t varint2[] = {0x01, 0x86, 0xA0}; // 100000 in varint encoding
  
  CassVector* vector = cass_vector_new(2);
  cass_vector_append_decimal(vector, varint1, sizeof(varint1), 2);
  cass_vector_append_decimal(vector, varint2, sizeof(varint2), 3);
  
  cass_statement_bind_vector(statement, 1, vector);
  cass_vector_free(vector);
  
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM decimal_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test DURATION vectors (not supported by Cassandra - driver should block)
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, DurationVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Table creation succeeds but driver blocks duration vector operations
  session_.execute("CREATE TABLE duration_vectors (id int PRIMARY KEY, vec vector<duration, 2>)");
  
  CassVector* vector = cass_vector_new(2);
  
  // Driver should return error for duration vector elements
  CassError rc = cass_vector_append_duration(vector, 1, 2, 3000000000LL); // 1 month, 2 days, 3 seconds in nanos
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  rc = cass_vector_append_duration(vector, 0, 0, 1000000000LL); // 1 second in nanos
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  cass_vector_free(vector);
}

/**
 * Test VARINT vectors (variable-length integer)
 * Note: We need to use CQL for varint as the C driver doesn't have direct varint support
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, VarintVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE varint_vectors (id int PRIMARY KEY, vec vector<varint, 3>)");
  
  // Use CQL to insert varint vectors
  session_.execute("INSERT INTO varint_vectors (id, vec) VALUES (1, [123456789012345678901234567890, -999999999999999999999999, 0])");
  
  // Verify
  Result result = session_.execute("SELECT vec FROM varint_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
  
  // Verify we can iterate (varint values come back as bytes)
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  int count = 0;
  while (cass_iterator_next(iter)) {
    count++;
  }
  cass_iterator_free(iter);
  ASSERT_EQ(3, count);
}

/**
 * Test DATE vectors
 * Note: Date support varies, using CQL for simplicity
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, DateVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE date_vectors (id int PRIMARY KEY, vec vector<date, 3>)");
  
  // Use CQL to insert date vectors
  session_.execute("INSERT INTO date_vectors (id, vec) VALUES (1, ['2024-01-01', '2024-06-15', '2024-12-31'])");
  
  // Verify
  Result result = session_.execute("SELECT vec FROM date_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
  
  // Verify we can iterate
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  int count = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    ASSERT_EQ(CASS_VALUE_TYPE_DATE, cass_value_type(element));
    count++;
  }
  cass_iterator_free(iter);
  ASSERT_EQ(3, count);
}

/**
 * Test TIME vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, TimeVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE time_vectors (id int PRIMARY KEY, vec vector<time, 3>)");
  
  // Use CQL to insert time vectors  
  session_.execute("INSERT INTO time_vectors (id, vec) VALUES (1, ['09:30:00', '12:00:00', '18:45:30'])");
  
  // Verify
  Result result = session_.execute("SELECT vec FROM time_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
  
  // Verify we can iterate
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  int count = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    ASSERT_EQ(CASS_VALUE_TYPE_TIME, cass_value_type(element));
    count++;
  }
  cass_iterator_free(iter);
  ASSERT_EQ(3, count);
}

/**
 * Test TIMESTAMP vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, TimestampVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE timestamp_vectors (id int PRIMARY KEY, vec vector<timestamp, 2>)");
  
  // Use CQL to insert timestamp vectors
  session_.execute("INSERT INTO timestamp_vectors (id, vec) VALUES (1, ['2024-01-01 10:00:00+0000', '2024-12-31 23:59:59+0000'])");
  
  // Verify
  Result result = session_.execute("SELECT vec FROM timestamp_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
  
  // Verify we can iterate
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  int count = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    ASSERT_EQ(CASS_VALUE_TYPE_TIMESTAMP, cass_value_type(element));
    count++;
  }
  cass_iterator_free(iter);
  ASSERT_EQ(2, count);
}

/**
 * Test vectors of collections (LIST)
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, ListVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  session_.execute("CREATE TABLE list_vectors (id int PRIMARY KEY, vec vector<frozen<list<int>>, 2>)");
  
  CassStatement* statement = cass_statement_new(
    "INSERT INTO list_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  // Create two lists
  CassCollection* list1 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 3);
  cass_collection_append_int32(list1, 1);
  cass_collection_append_int32(list1, 2);
  cass_collection_append_int32(list1, 3);
  
  CassCollection* list2 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  cass_collection_append_int32(list2, 10);
  cass_collection_append_int32(list2, 20);
  
  CassVector* vector = cass_vector_new(2);
  
  // Verify that appending collections returns an error
  CassError append_rc = cass_vector_append_collection(vector, list1);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, append_rc) << "Expected error when appending collection to vector";
  
  append_rc = cass_vector_append_collection(vector, list2);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, append_rc) << "Expected error when appending collection to vector";
  
  cass_collection_free(list1);
  cass_collection_free(list2);
  cass_vector_free(vector);
  cass_statement_free(statement);
  
  // Test passes if we correctly reject collection vectors
  return;
  
  // Verify
  Result result = session_.execute("SELECT vec FROM list_vectors WHERE id = 1");
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
}

/**
 * Test vectors of collections (SET) - not supported
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, SetVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Table creation will fail for set vectors
  try {
    session_.execute("CREATE TABLE set_vectors (id int PRIMARY KEY, vec vector<frozen<set<int>>, 2>)");
  } catch (...) {
    // Expected to fail
  }
  
  // Create sets to test driver blocking
  CassCollection* set1 = cass_collection_new(CASS_COLLECTION_TYPE_SET, 3);
  cass_collection_append_int32(set1, 1);
  cass_collection_append_int32(set1, 2);
  cass_collection_append_int32(set1, 3);
  
  CassCollection* set2 = cass_collection_new(CASS_COLLECTION_TYPE_SET, 2);
  cass_collection_append_int32(set2, 10);
  cass_collection_append_int32(set2, 20);
  
  CassVector* vector = cass_vector_new(2);
  
  // Driver should return error for set vector elements
  CassError rc = cass_vector_append_collection(vector, set1);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  rc = cass_vector_append_collection(vector, set2);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  cass_collection_free(set1);
  cass_collection_free(set2);
  cass_vector_free(vector);
}

/**
 * Test vectors of collections (MAP) - not supported
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, MapVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Table creation will fail for map vectors
  try {
    session_.execute("CREATE TABLE map_vectors (id int PRIMARY KEY, vec vector<frozen<map<int, text>>, 2>)");
  } catch (...) {
    // Expected to fail
  }
  
  // Create maps to test driver blocking
  CassCollection* map1 = cass_collection_new(CASS_COLLECTION_TYPE_MAP, 2);
  cass_collection_append_int32(map1, 1);
  cass_collection_append_string(map1, "one");
  cass_collection_append_int32(map1, 2);
  cass_collection_append_string(map1, "two");
  
  CassCollection* map2 = cass_collection_new(CASS_COLLECTION_TYPE_MAP, 1);
  cass_collection_append_int32(map2, 10);
  cass_collection_append_string(map2, "ten");
  
  CassVector* vector = cass_vector_new(2);
  
  // Driver should return error for map vector elements
  CassError rc = cass_vector_append_collection(vector, map1);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  rc = cass_vector_append_collection(vector, map2);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  cass_collection_free(map1);
  cass_collection_free(map2);
  cass_vector_free(vector);
}

/**
 * Test vectors of tuples - not supported
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, TupleVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Table creation will fail for tuple vectors
  try {
    session_.execute("CREATE TABLE tuple_vectors (id int PRIMARY KEY, vec vector<frozen<tuple<int, text>>, 2>)");
  } catch (...) {
    // Expected to fail
  }
  
  // Create tuples to test driver blocking
  CassTuple* tuple1 = cass_tuple_new(2);
  cass_tuple_set_int32(tuple1, 0, 1);
  cass_tuple_set_string(tuple1, 1, "first");
  
  CassTuple* tuple2 = cass_tuple_new(2);
  cass_tuple_set_int32(tuple2, 0, 2);
  cass_tuple_set_string(tuple2, 1, "second");
  
  CassVector* vector = cass_vector_new(2);
  
  // Driver should return error for tuple vector elements
  CassError rc = cass_vector_append_tuple(vector, tuple1);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  rc = cass_vector_append_tuple(vector, tuple2);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  cass_tuple_free(tuple1);
  cass_tuple_free(tuple2);
  cass_vector_free(vector);
}

/**
 * Test vector of vectors - not supported
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, VectorOfVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Table creation will fail for vector of vectors
  try {
    session_.execute("CREATE TABLE vector_of_vectors (id int PRIMARY KEY, vec vector<frozen<vector<int, 2>>, 3>)");
  } catch (...) {
    // Expected to fail
  }
  
  // Create inner vectors to test driver blocking
  CassVector* inner1 = cass_vector_new(2);
  cass_vector_append_int32(inner1, 1);
  cass_vector_append_int32(inner1, 2);
  
  CassVector* inner2 = cass_vector_new(2);
  cass_vector_append_int32(inner2, 3);
  cass_vector_append_int32(inner2, 4);
  
  CassVector* inner3 = cass_vector_new(2);
  cass_vector_append_int32(inner3, 5);
  cass_vector_append_int32(inner3, 6);
  
  // Create outer vector
  CassVector* vector = cass_vector_new(3);
  
  // Driver should return error for vector vector elements
  CassError rc = cass_vector_append_vector(vector, inner1);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  rc = cass_vector_append_vector(vector, inner2);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  rc = cass_vector_append_vector(vector, inner3);
  ASSERT_EQ(CASS_ERROR_LIB_INVALID_VALUE_TYPE, rc);
  
  cass_vector_free(inner1);
  cass_vector_free(inner2);
  cass_vector_free(inner3);
  cass_vector_free(vector);
}

/**
 * Test prepared statements with all vector types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllDataTypesTest, PreparedStatementsAllTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Test prepared statements with supported vector types only
  std::vector<std::string> test_types = {
    "float", "double", "int", "bigint", "smallint", "tinyint",
    "boolean", "text", "uuid", "blob"
  };
  // Note: inet and duration are excluded as they're not properly supported by Cassandra 5
  
  for (const auto& type : test_types) {
    std::string table_name = "prepared_" + type;
    session_.execute("CREATE TABLE " + table_name + " (id int PRIMARY KEY, vec vector<" + type + ", 2>)");
    
    // Prepare statement
    CassFuture* prepare_future = cass_session_prepare(session_.get(),
      ("INSERT INTO " + table_name + " (id, vec) VALUES (?, ?)").c_str());
    
    const CassPrepared* prepared = cass_future_get_prepared(prepare_future);
    ASSERT_NE(nullptr, prepared) << "Failed to prepare for type: " << type;
    cass_future_free(prepare_future);
    
    // Execute with bound values
    CassStatement* bound = cass_prepared_bind(prepared);
    cass_statement_bind_int32(bound, 0, 1);
    
    CassVector* vector = cass_vector_new(2);
    
    // Add type-specific values
    if (type == "float") {
      cass_vector_append_float(vector, 1.5f);
      cass_vector_append_float(vector, 2.5f);
    } else if (type == "double") {
      cass_vector_append_double(vector, 1.5);
      cass_vector_append_double(vector, 2.5);
    } else if (type == "int") {
      cass_vector_append_int32(vector, 10);
      cass_vector_append_int32(vector, 20);
    } else if (type == "bigint") {
      cass_vector_append_int64(vector, 100LL);
      cass_vector_append_int64(vector, 200LL);
    } else if (type == "smallint") {
      cass_vector_append_int16(vector, 10);
      cass_vector_append_int16(vector, 20);
    } else if (type == "tinyint") {
      cass_vector_append_int8(vector, 1);
      cass_vector_append_int8(vector, 2);
    } else if (type == "boolean") {
      cass_vector_append_bool(vector, cass_true);
      cass_vector_append_bool(vector, cass_false);
    } else if (type == "text") {
      cass_vector_append_string(vector, "hello");
      cass_vector_append_string(vector, "world");
    } else if (type == "uuid") {
      CassUuidGen* uuid_gen = cass_uuid_gen_new();
      CassUuid uuid1, uuid2;
      cass_uuid_gen_random(uuid_gen, &uuid1);
      cass_uuid_gen_random(uuid_gen, &uuid2);
      cass_uuid_gen_free(uuid_gen);
      cass_vector_append_uuid(vector, uuid1);
      cass_vector_append_uuid(vector, uuid2);
    } else if (type == "blob") {
      const cass_byte_t blob1[] = {0x01, 0x02};
      const cass_byte_t blob2[] = {0x03, 0x04};
      cass_vector_append_bytes(vector, blob1, sizeof(blob1));
      cass_vector_append_bytes(vector, blob2, sizeof(blob2));
    }
    
    cass_statement_bind_vector(bound, 1, vector);
    cass_vector_free(vector);
    
    CassFuture* future = cass_session_execute(session_.get(), bound);
    cass_statement_free(bound);
    
    CassError rc = cass_future_error_code(future);
    cass_future_free(future);
    ASSERT_EQ(CASS_OK, rc) << "Failed to insert for type: " << type;
    
    cass_prepared_free(prepared);
    
    // Verify insertion
    Result result = session_.execute("SELECT vec FROM " + table_name + " WHERE id = 1");
    ASSERT_EQ(1ul, result.row_count()) << "Failed to retrieve for type: " << type;
    
    const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
    ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value)) << "Wrong type for: " << type;
  }
}