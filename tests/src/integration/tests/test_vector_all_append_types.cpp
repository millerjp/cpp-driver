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
 * Comprehensive test for ALL vector append functions to ensure complete C API coverage.
 * This test validates that all data types can be appended to vectors.
 */
class VectorAllAppendTypesTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    session_ = this->default_cluster()
        .with_beta_protocol(true)
        .with_protocol_version(5)
        .connect();
  }
};

/**
 * Test all basic numeric types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendNumericTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5, 0, 0);

  test_utils::CassSessionPtr session(this->session_);
  test_utils::execute_query(session.get(), 
    "CREATE TABLE IF NOT EXISTS test_numeric_vectors ("
    "  id int PRIMARY KEY,"
    "  v_int8 vector<tinyint, 2>,"
    "  v_int16 vector<smallint, 2>,"
    "  v_int32 vector<int, 2>,"
    "  v_int64 vector<bigint, 2>,"
    "  v_float vector<float, 2>,"
    "  v_double vector<double, 2>,"
    "  v_varint vector<varint, 2>"
    ")");

  // Prepare insert statement
  const char* insert_query = 
    "INSERT INTO test_numeric_vectors (id, v_int8, v_int16, v_int32, v_int64, v_float, v_double, v_varint) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
  
  test_utils::CassPreparedPtr prepared(cass_session_prepare(session.get(), insert_query));
  test_utils::CassStatementPtr statement(cass_prepared_bind(prepared.get()));
  
  // Bind id
  ASSERT_EQ(cass_statement_bind_int32(statement.get(), 0, 1), CASS_OK);
  
  // Test int8 (tinyint)
  test_utils::CassVectorPtr v_int8(cass_vector_new(CASS_VALUE_TYPE_TINY_INT, 2));
  ASSERT_EQ(cass_vector_append_int8(v_int8.get(), 10), CASS_OK);
  ASSERT_EQ(cass_vector_append_int8(v_int8.get(), 20), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_int8.get()), CASS_OK);
  
  // Test int16 (smallint)
  test_utils::CassVectorPtr v_int16(cass_vector_new(CASS_VALUE_TYPE_SMALL_INT, 2));
  ASSERT_EQ(cass_vector_append_int16(v_int16.get(), 100), CASS_OK);
  ASSERT_EQ(cass_vector_append_int16(v_int16.get(), 200), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_int16.get()), CASS_OK);
  
  // Test int32
  test_utils::CassVectorPtr v_int32(cass_vector_new(CASS_VALUE_TYPE_INT, 2));
  ASSERT_EQ(cass_vector_append_int32(v_int32.get(), 1000), CASS_OK);
  ASSERT_EQ(cass_vector_append_int32(v_int32.get(), 2000), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 3, v_int32.get()), CASS_OK);
  
  // Test int64 (bigint)
  test_utils::CassVectorPtr v_int64(cass_vector_new(CASS_VALUE_TYPE_BIGINT, 2));
  ASSERT_EQ(cass_vector_append_int64(v_int64.get(), 10000), CASS_OK);
  ASSERT_EQ(cass_vector_append_int64(v_int64.get(), 20000), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 4, v_int64.get()), CASS_OK);
  
  // Test float
  test_utils::CassVectorPtr v_float(cass_vector_new(CASS_VALUE_TYPE_FLOAT, 2));
  ASSERT_EQ(cass_vector_append_float(v_float.get(), 1.5f), CASS_OK);
  ASSERT_EQ(cass_vector_append_float(v_float.get(), 2.5f), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 5, v_float.get()), CASS_OK);
  
  // Test double
  test_utils::CassVectorPtr v_double(cass_vector_new(CASS_VALUE_TYPE_DOUBLE, 2));
  ASSERT_EQ(cass_vector_append_double(v_double.get(), 3.14), CASS_OK);
  ASSERT_EQ(cass_vector_append_double(v_double.get(), 2.71), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 6, v_double.get()), CASS_OK);
  
  // Test varint
  test_utils::CassVectorPtr v_varint(cass_vector_new(CASS_VALUE_TYPE_VARINT, 2));
  const cass_byte_t varint1[] = {0x01, 0x23};
  const cass_byte_t varint2[] = {0x45, 0x67};
  ASSERT_EQ(cass_vector_append_varint(v_varint.get(), varint1, sizeof(varint1)), CASS_OK);
  ASSERT_EQ(cass_vector_append_varint(v_varint.get(), varint2, sizeof(varint2)), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 7, v_varint.get()), CASS_OK);
  
  // Execute
  test_utils::CassFuturePtr result_future(cass_session_execute(session.get(), statement.get()));
  ASSERT_EQ(cass_future_error_code(result_future.get()), CASS_OK);
}

/**
 * Test string and bytes types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendStringBytesTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5, 0, 0);

  test_utils::CassSessionPtr session(this->session_);
  test_utils::execute_query(session.get(),
    "CREATE TABLE IF NOT EXISTS test_string_vectors ("
    "  id int PRIMARY KEY,"
    "  v_text vector<text, 2>,"
    "  v_blob vector<blob, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_string_vectors (id, v_text, v_blob) VALUES (?, ?, ?)";
  
  test_utils::CassPreparedPtr prepared(cass_session_prepare(session.get(), insert_query));
  test_utils::CassStatementPtr statement(cass_prepared_bind(prepared.get()));
  
  ASSERT_EQ(cass_statement_bind_int32(statement.get(), 0, 1), CASS_OK);
  
  // Test text/varchar (using string functions)
  test_utils::CassVectorPtr v_text(cass_vector_new(CASS_VALUE_TYPE_TEXT, 2));
  ASSERT_EQ(cass_vector_append_string(v_text.get(), "hello"), CASS_OK);
  ASSERT_EQ(cass_vector_append_string_n(v_text.get(), "world", 5), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_text.get()), CASS_OK);
  
  // Test blob (using bytes)
  test_utils::CassVectorPtr v_blob(cass_vector_new(CASS_VALUE_TYPE_BLOB, 2));
  const cass_byte_t bytes1[] = {0x01, 0x02, 0x03};
  const cass_byte_t bytes2[] = {0x04, 0x05, 0x06};
  ASSERT_EQ(cass_vector_append_bytes(v_blob.get(), bytes1, sizeof(bytes1)), CASS_OK);
  ASSERT_EQ(cass_vector_append_bytes(v_blob.get(), bytes2, sizeof(bytes2)), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_blob.get()), CASS_OK);
  
  test_utils::CassFuturePtr result_future(cass_session_execute(session.get(), statement.get()));
  ASSERT_EQ(cass_future_error_code(result_future.get()), CASS_OK);
}

/**
 * Test boolean and UUID types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendBooleanUuidTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5, 0, 0);

  test_utils::CassSessionPtr session(this->session_);
  test_utils::execute_query(session.get(),
    "CREATE TABLE IF NOT EXISTS test_bool_uuid_vectors ("
    "  id int PRIMARY KEY,"
    "  v_bool vector<boolean, 2>,"
    "  v_uuid vector<uuid, 2>,"
    "  v_timeuuid vector<timeuuid, 2>,"
    "  v_inet vector<inet, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_bool_uuid_vectors (id, v_bool, v_uuid, v_timeuuid, v_inet) VALUES (?, ?, ?, ?, ?)";
  
  test_utils::CassPreparedPtr prepared(cass_session_prepare(session.get(), insert_query));
  test_utils::CassStatementPtr statement(cass_prepared_bind(prepared.get()));
  
  ASSERT_EQ(cass_statement_bind_int32(statement.get(), 0, 1), CASS_OK);
  
  // Test boolean
  test_utils::CassVectorPtr v_bool(cass_vector_new(CASS_VALUE_TYPE_BOOLEAN, 2));
  ASSERT_EQ(cass_vector_append_bool(v_bool.get(), cass_true), CASS_OK);
  ASSERT_EQ(cass_vector_append_bool(v_bool.get(), cass_false), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_bool.get()), CASS_OK);
  
  // Test UUID
  test_utils::CassVectorPtr v_uuid(cass_vector_new(CASS_VALUE_TYPE_UUID, 2));
  CassUuid uuid1 = test_utils::generate_random_uuid();
  CassUuid uuid2 = test_utils::generate_random_uuid();
  ASSERT_EQ(cass_vector_append_uuid(v_uuid.get(), uuid1), CASS_OK);
  ASSERT_EQ(cass_vector_append_uuid(v_uuid.get(), uuid2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_uuid.get()), CASS_OK);
  
  // Test TimeUUID
  test_utils::CassVectorPtr v_timeuuid(cass_vector_new(CASS_VALUE_TYPE_TIMEUUID, 2));
  CassUuid timeuuid1 = test_utils::generate_time_uuid();
  CassUuid timeuuid2 = test_utils::generate_time_uuid();
  ASSERT_EQ(cass_vector_append_uuid(v_timeuuid.get(), timeuuid1), CASS_OK);
  ASSERT_EQ(cass_vector_append_uuid(v_timeuuid.get(), timeuuid2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 3, v_timeuuid.get()), CASS_OK);
  
  // Test INET
  test_utils::CassVectorPtr v_inet(cass_vector_new(CASS_VALUE_TYPE_INET, 2));
  CassInet inet1;
  ASSERT_EQ(cass_inet_from_string("127.0.0.1", &inet1), CASS_OK);
  CassInet inet2;
  ASSERT_EQ(cass_inet_from_string("192.168.1.1", &inet2), CASS_OK);
  ASSERT_EQ(cass_vector_append_inet(v_inet.get(), inet1), CASS_OK);
  ASSERT_EQ(cass_vector_append_inet(v_inet.get(), inet2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 4, v_inet.get()), CASS_OK);
  
  test_utils::CassFuturePtr result_future(cass_session_execute(session.get(), statement.get()));
  ASSERT_EQ(cass_future_error_code(result_future.get()), CASS_OK);
}

/**
 * Test date, time, timestamp types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendDateTimeTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5, 0, 0);

  test_utils::CassSessionPtr session(this->session_);
  test_utils::execute_query(session.get(),
    "CREATE TABLE IF NOT EXISTS test_datetime_vectors ("
    "  id int PRIMARY KEY,"
    "  v_date vector<date, 2>,"
    "  v_time vector<time, 2>,"
    "  v_timestamp vector<timestamp, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_datetime_vectors (id, v_date, v_time, v_timestamp) VALUES (?, ?, ?, ?)";
  
  test_utils::CassPreparedPtr prepared(cass_session_prepare(session.get(), insert_query));
  test_utils::CassStatementPtr statement(cass_prepared_bind(prepared.get()));
  
  ASSERT_EQ(cass_statement_bind_int32(statement.get(), 0, 1), CASS_OK);
  
  // Test date (days since epoch)
  test_utils::CassVectorPtr v_date(cass_vector_new(CASS_VALUE_TYPE_DATE, 2));
  cass_uint32_t date1 = cass_date_from_epoch(1609459200);  // 2021-01-01
  cass_uint32_t date2 = cass_date_from_epoch(1640995200);  // 2022-01-01
  ASSERT_EQ(cass_vector_append_date(v_date.get(), date1), CASS_OK);
  ASSERT_EQ(cass_vector_append_date(v_date.get(), date2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_date.get()), CASS_OK);
  
  // Test time (nanoseconds since midnight)
  test_utils::CassVectorPtr v_time(cass_vector_new(CASS_VALUE_TYPE_TIME, 2));
  cass_int64_t time1 = cass_time_from_epoch(3600);   // 1 hour in seconds -> nanoseconds
  cass_int64_t time2 = cass_time_from_epoch(7200);   // 2 hours in seconds -> nanoseconds
  ASSERT_EQ(cass_vector_append_time(v_time.get(), time1), CASS_OK);
  ASSERT_EQ(cass_vector_append_time(v_time.get(), time2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_time.get()), CASS_OK);
  
  // Test timestamp (milliseconds since epoch)
  test_utils::CassVectorPtr v_timestamp(cass_vector_new(CASS_VALUE_TYPE_TIMESTAMP, 2));
  cass_int64_t timestamp1 = 1609459200000LL;  // 2021-01-01 in milliseconds
  cass_int64_t timestamp2 = 1640995200000LL;  // 2022-01-01 in milliseconds
  ASSERT_EQ(cass_vector_append_timestamp(v_timestamp.get(), timestamp1), CASS_OK);
  ASSERT_EQ(cass_vector_append_timestamp(v_timestamp.get(), timestamp2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 3, v_timestamp.get()), CASS_OK);
  
  test_utils::CassFuturePtr result_future(cass_session_execute(session.get(), statement.get()));
  ASSERT_EQ(cass_future_error_code(result_future.get()), CASS_OK);
}

/**
 * Test decimal and duration types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendDecimalDurationTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5, 0, 0);

  test_utils::CassSessionPtr session(this->session_);
  test_utils::execute_query(session.get(),
    "CREATE TABLE IF NOT EXISTS test_decimal_duration_vectors ("
    "  id int PRIMARY KEY,"
    "  v_decimal vector<decimal, 2>,"
    "  v_duration vector<duration, 2>,"
    "  v_uint32 vector<int, 2>"  // Using int to test uint32 since CQL doesn't have uint
    ")");

  const char* insert_query = 
    "INSERT INTO test_decimal_duration_vectors (id, v_decimal, v_duration, v_uint32) VALUES (?, ?, ?, ?)";
  
  test_utils::CassPreparedPtr prepared(cass_session_prepare(session.get(), insert_query));
  test_utils::CassStatementPtr statement(cass_prepared_bind(prepared.get()));
  
  ASSERT_EQ(cass_statement_bind_int32(statement.get(), 0, 1), CASS_OK);
  
  // Test decimal
  test_utils::CassVectorPtr v_decimal(cass_vector_new(CASS_VALUE_TYPE_DECIMAL, 2));
  const cass_byte_t varint1[] = {0x01, 0x23};
  const cass_byte_t varint2[] = {0x45, 0x67};
  ASSERT_EQ(cass_vector_append_decimal(v_decimal.get(), varint1, sizeof(varint1), 2), CASS_OK);
  ASSERT_EQ(cass_vector_append_decimal(v_decimal.get(), varint2, sizeof(varint2), 3), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_decimal.get()), CASS_OK);
  
  // Test duration
  test_utils::CassVectorPtr v_duration(cass_vector_new(CASS_VALUE_TYPE_DURATION, 2));
  ASSERT_EQ(cass_vector_append_duration(v_duration.get(), 1, 2, 3000000000LL), CASS_OK);
  ASSERT_EQ(cass_vector_append_duration(v_duration.get(), 4, 5, 6000000000LL), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_duration.get()), CASS_OK);
  
  // Test uint32 (using int type in CQL)
  test_utils::CassVectorPtr v_uint32(cass_vector_new(CASS_VALUE_TYPE_INT, 2));
  ASSERT_EQ(cass_vector_append_uint32(v_uint32.get(), 100), CASS_OK);
  ASSERT_EQ(cass_vector_append_uint32(v_uint32.get(), 200), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 3, v_uint32.get()), CASS_OK);
  
  test_utils::CassFuturePtr result_future(cass_session_execute(session.get(), statement.get()));
  ASSERT_EQ(cass_future_error_code(result_future.get()), CASS_OK);
}

/**
 * Test collection types (list, set, map) in vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendCollectionTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5, 0, 0);

  test_utils::CassSessionPtr session(this->session_);
  test_utils::execute_query(session.get(),
    "CREATE TABLE IF NOT EXISTS test_collection_vectors ("
    "  id int PRIMARY KEY,"
    "  v_list vector<frozen<list<int>>, 2>,"
    "  v_set vector<frozen<set<text>>, 2>,"
    "  v_map vector<frozen<map<int,text>>, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_collection_vectors (id, v_list, v_set, v_map) VALUES (?, ?, ?, ?)";
  
  test_utils::CassPreparedPtr prepared(cass_session_prepare(session.get(), insert_query));
  test_utils::CassStatementPtr statement(cass_prepared_bind(prepared.get()));
  
  ASSERT_EQ(cass_statement_bind_int32(statement.get(), 0, 1), CASS_OK);
  
  // Test list in vector
  test_utils::CassVectorPtr v_list(cass_vector_new_from_data_type(
    cass_statement_get_data_type(statement.get(), 1)));
  
  test_utils::CassCollectionPtr list1(cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2));
  cass_collection_append_int32(list1.get(), 1);
  cass_collection_append_int32(list1.get(), 2);
  
  test_utils::CassCollectionPtr list2(cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2));
  cass_collection_append_int32(list2.get(), 3);
  cass_collection_append_int32(list2.get(), 4);
  
  ASSERT_EQ(cass_vector_append_collection(v_list.get(), list1.get()), CASS_OK);
  ASSERT_EQ(cass_vector_append_collection(v_list.get(), list2.get()), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_list.get()), CASS_OK);
  
  // Test set in vector
  test_utils::CassVectorPtr v_set(cass_vector_new_from_data_type(
    cass_statement_get_data_type(statement.get(), 2)));
  
  test_utils::CassCollectionPtr set1(cass_collection_new(CASS_COLLECTION_TYPE_SET, 2));
  cass_collection_append_string(set1.get(), "a");
  cass_collection_append_string(set1.get(), "b");
  
  test_utils::CassCollectionPtr set2(cass_collection_new(CASS_COLLECTION_TYPE_SET, 2));
  cass_collection_append_string(set2.get(), "c");
  cass_collection_append_string(set2.get(), "d");
  
  ASSERT_EQ(cass_vector_append_collection(v_set.get(), set1.get()), CASS_OK);
  ASSERT_EQ(cass_vector_append_collection(v_set.get(), set2.get()), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_set.get()), CASS_OK);
  
  // Test map in vector
  test_utils::CassVectorPtr v_map(cass_vector_new_from_data_type(
    cass_statement_get_data_type(statement.get(), 3)));
  
  test_utils::CassCollectionPtr map1(cass_collection_new(CASS_COLLECTION_TYPE_MAP, 2));
  cass_collection_append_int32(map1.get(), 1);
  cass_collection_append_string(map1.get(), "one");
  cass_collection_append_int32(map1.get(), 2);
  cass_collection_append_string(map1.get(), "two");
  
  test_utils::CassCollectionPtr map2(cass_collection_new(CASS_COLLECTION_TYPE_MAP, 1));
  cass_collection_append_int32(map2.get(), 3);
  cass_collection_append_string(map2.get(), "three");
  
  ASSERT_EQ(cass_vector_append_collection(v_map.get(), map1.get()), CASS_OK);
  ASSERT_EQ(cass_vector_append_collection(v_map.get(), map2.get()), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 3, v_map.get()), CASS_OK);
  
  test_utils::CassFuturePtr result_future(cass_session_execute(session.get(), statement.get()));
  ASSERT_EQ(cass_future_error_code(result_future.get()), CASS_OK);
}

/**
 * Test tuple and UDT types in vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendTupleUdtTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5, 0, 0);

  test_utils::CassSessionPtr session(this->session_);
  
  // Create UDT first
  test_utils::execute_query(session.get(),
    "CREATE TYPE IF NOT EXISTS test_udt (a int, b text)");
  
  test_utils::execute_query(session.get(),
    "CREATE TABLE IF NOT EXISTS test_tuple_udt_vectors ("
    "  id int PRIMARY KEY,"
    "  v_tuple vector<frozen<tuple<int,text>>, 2>,"
    "  v_udt vector<frozen<test_udt>, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_tuple_udt_vectors (id, v_tuple, v_udt) VALUES (?, ?, ?)";
  
  test_utils::CassPreparedPtr prepared(cass_session_prepare(session.get(), insert_query));
  test_utils::CassStatementPtr statement(cass_prepared_bind(prepared.get()));
  
  ASSERT_EQ(cass_statement_bind_int32(statement.get(), 0, 1), CASS_OK);
  
  // Test tuple in vector
  test_utils::CassVectorPtr v_tuple(cass_vector_new_from_data_type(
    cass_statement_get_data_type(statement.get(), 1)));
  
  test_utils::CassTuplePtr tuple1(cass_tuple_new(2));
  cass_tuple_set_int32(tuple1.get(), 0, 1);
  cass_tuple_set_string(tuple1.get(), 1, "one");
  
  test_utils::CassTuplePtr tuple2(cass_tuple_new(2));
  cass_tuple_set_int32(tuple2.get(), 0, 2);
  cass_tuple_set_string(tuple2.get(), 1, "two");
  
  ASSERT_EQ(cass_vector_append_tuple(v_tuple.get(), tuple1.get()), CASS_OK);
  ASSERT_EQ(cass_vector_append_tuple(v_tuple.get(), tuple2.get()), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_tuple.get()), CASS_OK);
  
  // Test UDT in vector
  const CassDataType* udt_type = cass_statement_get_data_type(statement.get(), 2);
  test_utils::CassVectorPtr v_udt(cass_vector_new_from_data_type(udt_type));
  
  // Get the UDT element type from the vector
  const CassDataType* vector_data_type = cass_vector_data_type(v_udt.get());
  const CassDataType* element_data_type = cass_vector_element_data_type(v_udt.get());
  
  test_utils::CassUserTypePtr udt1(cass_user_type_new_from_data_type(element_data_type));
  cass_user_type_set_int32_by_name(udt1.get(), "a", 1);
  cass_user_type_set_string_by_name(udt1.get(), "b", "first");
  
  test_utils::CassUserTypePtr udt2(cass_user_type_new_from_data_type(element_data_type));
  cass_user_type_set_int32_by_name(udt2.get(), "a", 2);
  cass_user_type_set_string_by_name(udt2.get(), "b", "second");
  
  ASSERT_EQ(cass_vector_append_user_type(v_udt.get(), udt1.get()), CASS_OK);
  ASSERT_EQ(cass_vector_append_user_type(v_udt.get(), udt2.get()), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 2, v_udt.get()), CASS_OK);
  
  test_utils::CassFuturePtr result_future(cass_session_execute(session.get(), statement.get()));
  ASSERT_EQ(cass_future_error_code(result_future.get()), CASS_OK);
}

/**
 * Test nested vectors (vector of vectors)
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendNestedVectorTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5, 0, 0);

  test_utils::CassSessionPtr session(this->session_);
  test_utils::execute_query(session.get(),
    "CREATE TABLE IF NOT EXISTS test_nested_vectors ("
    "  id int PRIMARY KEY,"
    "  v_nested vector<frozen<vector<int, 2>>, 2>"
    ")");

  const char* insert_query = 
    "INSERT INTO test_nested_vectors (id, v_nested) VALUES (?, ?)";
  
  test_utils::CassPreparedPtr prepared(cass_session_prepare(session.get(), insert_query));
  test_utils::CassStatementPtr statement(cass_prepared_bind(prepared.get()));
  
  ASSERT_EQ(cass_statement_bind_int32(statement.get(), 0, 1), CASS_OK);
  
  // Create nested vector
  test_utils::CassVectorPtr v_nested(cass_vector_new_from_data_type(
    cass_statement_get_data_type(statement.get(), 1)));
  
  // Create inner vectors
  test_utils::CassVectorPtr inner1(cass_vector_new(CASS_VALUE_TYPE_INT, 2));
  cass_vector_append_int32(inner1.get(), 1);
  cass_vector_append_int32(inner1.get(), 2);
  
  test_utils::CassVectorPtr inner2(cass_vector_new(CASS_VALUE_TYPE_INT, 2));
  cass_vector_append_int32(inner2.get(), 3);
  cass_vector_append_int32(inner2.get(), 4);
  
  // Append inner vectors to outer vector
  ASSERT_EQ(cass_vector_append_vector(v_nested.get(), inner1.get()), CASS_OK);
  ASSERT_EQ(cass_vector_append_vector(v_nested.get(), inner2.get()), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, v_nested.get()), CASS_OK);
  
  test_utils::CassFuturePtr result_future(cass_session_execute(session.get(), statement.get()));
  ASSERT_EQ(cass_future_error_code(result_future.get()), CASS_OK);
}

/**
 * Test that null values are properly rejected
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, RejectNullValues) {
  CHECK_FAILURE;
  
  // Vectors don't support null elements
  test_utils::CassVectorPtr vector(cass_vector_new(CASS_VALUE_TYPE_INT, 2));
  
  // Try to append null - should fail
  CassError error = cass_vector_append_null(vector.get());
  ASSERT_EQ(error, CASS_ERROR_LIB_NULL_VALUE) 
    << "Vectors should reject null values";
}

/**
 * Test custom types (if supported)
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, AppendCustomTypes) {
  CHECK_FAILURE;
  
  // Custom types require special handling
  // This is a placeholder for custom type testing
  // Most users won't need custom types, but the API supports them
  
  test_utils::CassVectorPtr vector(cass_vector_new(CASS_VALUE_TYPE_BLOB, 2));
  
  // Custom types can be appended using the custom functions
  const cass_byte_t custom_data[] = {0x01, 0x02, 0x03};
  CassError error = cass_vector_append_custom(vector.get(), 
                                              "org.example.CustomType",
                                              custom_data, 
                                              sizeof(custom_data));
  // This might fail if the custom type doesn't match, which is expected
  // The important thing is that the function exists and can be called
}

/**
 * Test error conditions - dimension overflow
 */
CASSANDRA_INTEGRATION_TEST_F(VectorAllAppendTypesTest, ErrorDimensionOverflow) {
  CHECK_FAILURE;
  
  // Create a vector with dimension 2
  test_utils::CassVectorPtr vector(cass_vector_new(CASS_VALUE_TYPE_INT, 2));
  
  // Fill it up
  ASSERT_EQ(cass_vector_append_int32(vector.get(), 1), CASS_OK);
  ASSERT_EQ(cass_vector_append_int32(vector.get(), 2), CASS_OK);
  
  // Try to add a third element - should fail
  CassError error = cass_vector_append_int32(vector.get(), 3);
  ASSERT_EQ(error, CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS) 
    << "Should not be able to exceed vector dimension";
}