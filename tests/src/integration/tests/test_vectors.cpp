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
#include <limits>

/**
 * Vector data type integration tests
 * 
 * These tests verify the vector data type support introduced in Cassandra 5.0.
 * They cover all primitive types, dimension limits, error cases, and vector iteration.
 */
class VectorTests : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Vectors require Cassandra 5.0+
    if (!Options::is_cassandra() || server_version_ < "5.0.0") {
      SKIP_TEST("Vector types require Cassandra 5.0+");
    }
    
    session_.execute(
        format_string("CREATE KEYSPACE IF NOT EXISTS %s "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}", 
                     keyspace_name_.c_str()));
    session_.execute("USE " + keyspace_name_);
  }
};

/**
 * Test all primitive vector types with extreme values
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result All vector types work with extreme values including negative numbers
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTests, AllPrimitiveTypes) {
  CHECK_FAILURE;
  CHECK_VERSION("5.0.0");
  
  // Create comprehensive table
  session_.execute(
      "CREATE TABLE vector_primitives ("
      "  id int PRIMARY KEY,"
      "  v_tinyint vector<tinyint, 3>,"
      "  v_smallint vector<smallint, 3>,"
      "  v_int vector<int, 3>,"
      "  v_bigint vector<bigint, 3>,"
      "  v_float vector<float, 3>,"
      "  v_double vector<double, 3>,"
      "  v_boolean vector<boolean, 3>,"
      "  v_text vector<text, 3>,"
      "  v_varchar vector<varchar, 3>,"
      "  v_ascii vector<ascii, 3>,"
      "  v_blob vector<blob, 3>,"
      "  v_uuid vector<uuid, 2>,"
      "  v_inet vector<inet, 2>,"
      "  v_timestamp vector<timestamp, 2>,"
      "  v_date vector<date, 2>,"
      "  v_time vector<time, 2>"
      ")");

  // Prepare insert statement
  Prepared prepared = session_.prepare(
      "INSERT INTO vector_primitives ("
      "  id, v_tinyint, v_smallint, v_int, v_bigint,"
      "  v_float, v_double, v_boolean, v_text, v_varchar,"
      "  v_ascii, v_blob, v_uuid, v_inet, v_timestamp,"
      "  v_date, v_time"
      ") VALUES ("
      "  ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?"
      ")");

  Statement statement = prepared.bind();
  cass_statement_bind_int32(statement.get(), 0, 1);

  // v_tinyint: extreme values [-128, 0, 127]
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 1);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_int8(vec, std::numeric_limits<int8_t>::min()), CASS_OK);
    ASSERT_EQ(cass_vector_append_int8(vec, 0), CASS_OK);
    ASSERT_EQ(cass_vector_append_int8(vec, std::numeric_limits<int8_t>::max()), CASS_OK);
    cass_statement_bind_vector(statement.get(), 1, vec);
    cass_vector_free(vec);
  }

  // v_smallint: extreme values [-32768, -1000, 32767]
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 2);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_int16(vec, std::numeric_limits<int16_t>::min()), CASS_OK);
    ASSERT_EQ(cass_vector_append_int16(vec, -1000), CASS_OK);
    ASSERT_EQ(cass_vector_append_int16(vec, std::numeric_limits<int16_t>::max()), CASS_OK);
    cass_statement_bind_vector(statement.get(), 2, vec);
    cass_vector_free(vec);
  }

  // v_int: extreme values [INT_MIN, -1000000, INT_MAX]
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 3);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_int32(vec, std::numeric_limits<int32_t>::min()), CASS_OK);
    ASSERT_EQ(cass_vector_append_int32(vec, -1000000), CASS_OK);
    ASSERT_EQ(cass_vector_append_int32(vec, std::numeric_limits<int32_t>::max()), CASS_OK);
    cass_statement_bind_vector(statement.get(), 3, vec);
    cass_vector_free(vec);
  }

  // v_bigint: extreme values [LONG_MIN, -1T, LONG_MAX]
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 4);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_int64(vec, std::numeric_limits<int64_t>::min()), CASS_OK);
    ASSERT_EQ(cass_vector_append_int64(vec, -1000000000000LL), CASS_OK);
    ASSERT_EQ(cass_vector_append_int64(vec, std::numeric_limits<int64_t>::max()), CASS_OK);
    cass_statement_bind_vector(statement.get(), 4, vec);
    cass_vector_free(vec);
  }

  // v_float: negative, zero, positive with precision
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 5);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_float(vec, -123.456789f), CASS_OK);
    ASSERT_EQ(cass_vector_append_float(vec, 0.0f), CASS_OK);
    ASSERT_EQ(cass_vector_append_float(vec, 3.14159265f), CASS_OK);
    cass_statement_bind_vector(statement.get(), 5, vec);
    cass_vector_free(vec);
  }

  // v_double: large range values
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 6);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_double(vec, -999999999.999999999), CASS_OK);
    ASSERT_EQ(cass_vector_append_double(vec, 0.0), CASS_OK);
    ASSERT_EQ(cass_vector_append_double(vec, 999999999.999999999), CASS_OK);
    cass_statement_bind_vector(statement.get(), 6, vec);
    cass_vector_free(vec);
  }

  // v_boolean: all combinations
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 7);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_bool(vec, cass_false), CASS_OK);
    ASSERT_EQ(cass_vector_append_bool(vec, cass_true), CASS_OK);
    ASSERT_EQ(cass_vector_append_bool(vec, cass_false), CASS_OK);
    cass_statement_bind_vector(statement.get(), 7, vec);
    cass_vector_free(vec);
  }

  // v_text: various strings including empty and UTF-8
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 8);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_string(vec, ""), CASS_OK);  // empty string
    ASSERT_EQ(cass_vector_append_string(vec, "Hello, 世界!"), CASS_OK);  // UTF-8
    ASSERT_EQ(cass_vector_append_string(vec, "Test123"), CASS_OK);
    cass_statement_bind_vector(statement.get(), 8, vec);
    cass_vector_free(vec);
  }

  // v_varchar
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 9);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_string(vec, "varchar1"), CASS_OK);
    ASSERT_EQ(cass_vector_append_string(vec, "varchar2"), CASS_OK);
    ASSERT_EQ(cass_vector_append_string(vec, "varchar3"), CASS_OK);
    cass_statement_bind_vector(statement.get(), 9, vec);
    cass_vector_free(vec);
  }

  // v_ascii
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 10);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_string(vec, "ASCII1"), CASS_OK);
    ASSERT_EQ(cass_vector_append_string(vec, "ASCII2"), CASS_OK);
    ASSERT_EQ(cass_vector_append_string(vec, "ASCII3"), CASS_OK);
    cass_statement_bind_vector(statement.get(), 10, vec);
    cass_vector_free(vec);
  }

  // v_blob: binary data including null bytes
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 11);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    cass_byte_t blob1[] = {0x00, 0x00, 0x00, 0x00};  // all zeros
    cass_byte_t blob2[] = {0xFF, 0xFF, 0xFF, 0xFF};  // all ones
    cass_byte_t blob3[] = {0xDE, 0xAD, 0xBE, 0xEF};  // pattern
    ASSERT_EQ(cass_vector_append_bytes(vec, blob1, 4), CASS_OK);
    ASSERT_EQ(cass_vector_append_bytes(vec, blob2, 4), CASS_OK);
    ASSERT_EQ(cass_vector_append_bytes(vec, blob3, 4), CASS_OK);
    cass_statement_bind_vector(statement.get(), 11, vec);
    cass_vector_free(vec);
  }

  // v_uuid: different UUID values
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 12);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    CassUuid uuid1, uuid2;
    cass_uuid_gen_time(cass_uuid_gen_new(), &uuid1);
    cass_uuid_gen_random(cass_uuid_gen_new(), &uuid2);
    ASSERT_EQ(cass_vector_append_uuid(vec, uuid1), CASS_OK);
    ASSERT_EQ(cass_vector_append_uuid(vec, uuid2), CASS_OK);
    cass_statement_bind_vector(statement.get(), 12, vec);
    cass_vector_free(vec);
  }

  // v_inet: IPv4 and IPv6
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 13);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    CassInet inet1, inet2;
    cass_inet_from_string("192.168.1.1", &inet1);
    cass_inet_from_string("::1", &inet2);  // IPv6 loopback
    ASSERT_EQ(cass_vector_append_inet(vec, inet1), CASS_OK);
    ASSERT_EQ(cass_vector_append_inet(vec, inet2), CASS_OK);
    cass_statement_bind_vector(statement.get(), 13, vec);
    cass_vector_free(vec);
  }

  // v_timestamp
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 14);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_int64(vec, 0LL), CASS_OK);  // epoch
    ASSERT_EQ(cass_vector_append_int64(vec, 1704067200000LL), CASS_OK);  // 2024-01-01
    cass_statement_bind_vector(statement.get(), 14, vec);
    cass_vector_free(vec);
  }

  // v_date
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 15);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_uint32(vec, 0), CASS_OK);  // epoch date
    ASSERT_EQ(cass_vector_append_uint32(vec, 2147483647), CASS_OK);  // max date
    cass_statement_bind_vector(statement.get(), 15, vec);
    cass_vector_free(vec);
  }

  // v_time
  {
    const CassDataType* type = cass_prepared_parameter_data_type(prepared.get(), 16);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_int64(vec, 0LL), CASS_OK);  // midnight
    ASSERT_EQ(cass_vector_append_int64(vec, 86399999999999LL), CASS_OK);  // end of day
    cass_statement_bind_vector(statement.get(), 16, vec);
    cass_vector_free(vec);
  }

  // Execute the insert
  session_.execute(statement);

  // Read back and validate
  Result result = session_.execute("SELECT * FROM vector_primitives WHERE id = 1");
  ASSERT_EQ(result.row_count(), 1u);
  Row row = result.first_row();

  // Validate v_tinyint with extreme values
  {
    const CassValue* value = cass_row_get_column_by_name(row.get(), "v_tinyint");
    ASSERT_NE(value, nullptr);
    ASSERT_FALSE(cass_value_is_null(value));
    CassIterator* iter = cass_iterator_from_vector(value);
    ASSERT_NE(iter, nullptr);
    
    std::vector<int8_t> values;
    while (cass_iterator_next(iter)) {
      const CassValue* elem = cass_iterator_get_value(iter);
      int8_t val;
      ASSERT_EQ(cass_value_get_int8(elem, &val), CASS_OK);
      values.push_back(val);
    }
    cass_iterator_free(iter);
    
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], std::numeric_limits<int8_t>::min());
    EXPECT_EQ(values[1], 0);
    EXPECT_EQ(values[2], std::numeric_limits<int8_t>::max());
  }

  // Validate v_float with negative values
  {
    const CassValue* value = cass_row_get_column_by_name(row.get(), "v_float");
    ASSERT_NE(value, nullptr);
    ASSERT_FALSE(cass_value_is_null(value));
    CassIterator* iter = cass_iterator_from_vector(value);
    ASSERT_NE(iter, nullptr);
    
    std::vector<float> values;
    while (cass_iterator_next(iter)) {
      const CassValue* elem = cass_iterator_get_value(iter);
      float val;
      ASSERT_EQ(cass_value_get_float(elem, &val), CASS_OK);
      values.push_back(val);
    }
    cass_iterator_free(iter);
    
    ASSERT_EQ(values.size(), 3u);
    EXPECT_NEAR(values[0], -123.456789f, 0.0001f);
    EXPECT_EQ(values[1], 0.0f);
    EXPECT_NEAR(values[2], 3.14159265f, 0.0001f);
  }

  // Validate v_text with empty string and UTF-8
  {
    const CassValue* value = cass_row_get_column_by_name(row.get(), "v_text");
    ASSERT_NE(value, nullptr);
    ASSERT_FALSE(cass_value_is_null(value));
    CassIterator* iter = cass_iterator_from_vector(value);
    ASSERT_NE(iter, nullptr);
    
    std::vector<std::string> values;
    while (cass_iterator_next(iter)) {
      const CassValue* elem = cass_iterator_get_value(iter);
      const char* str;
      size_t str_len;
      ASSERT_EQ(cass_value_get_string(elem, &str, &str_len), CASS_OK);
      values.push_back(std::string(str, str_len));
    }
    cass_iterator_free(iter);
    
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], "");
    EXPECT_EQ(values[1], "Hello, 世界!");
    EXPECT_EQ(values[2], "Test123");
  }
}

/**
 * Test iteration over all vector types
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result All vector types can be iterated correctly
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTests, IterateAllTypes) {
  CHECK_FAILURE;
  CHECK_VERSION("5.0.0");
  
  // Create table with various vector types
  session_.execute(
      "CREATE TABLE vector_iteration ("
      "  id int PRIMARY KEY,"
      "  v_int vector<int, 5>,"
      "  v_float vector<float, 4>,"
      "  v_text vector<text, 3>,"
      "  v_boolean vector<boolean, 3>,"
      "  v_bigint vector<bigint, 2>,"
      "  v_double vector<double, 2>,"
      "  v_smallint vector<smallint, 6>,"
      "  v_tinyint vector<tinyint, 7>,"
      "  v_blob vector<blob, 2>,"
      "  v_uuid vector<uuid, 2>"
      ")");

  // Insert test data using simple statement with literal values
  session_.execute(
      "INSERT INTO vector_iteration (id, v_int, v_float, v_text, v_boolean, "
      "v_bigint, v_double, v_smallint, v_tinyint, v_blob, v_uuid) VALUES ("
      "1, "
      "[10, 20, 30, 40, 50], "
      "[1.1, 2.2, 3.3, 4.4], "
      "['hello', 'world', 'test'], "
      "[true, false, true], "
      "[1000000, 2000000], "
      "[1.111, 2.222], "
      "[100, 200, 300, 400, 500, 600], "
      "[1, 2, 3, 4, 5, 6, 7], "
      "[0xDEAD, 0xBEEF], "
      "[550e8400-e29b-41d4-a716-446655440000, 123e4567-e89b-12d3-a456-426614174000]"
      ")");

  // Read and iterate over each vector type
  Result result = session_.execute("SELECT * FROM vector_iteration WHERE id = 1");
  ASSERT_EQ(result.row_count(), 1u);
  Row row = result.first_row();

  // Iterate v_int
  {
    const CassValue* value = cass_row_get_column_by_name(row.get(), "v_int");
    ASSERT_NE(value, nullptr);
    ASSERT_FALSE(cass_value_is_null(value));
    CassIterator* iter = cass_iterator_from_vector(value);
    ASSERT_NE(iter, nullptr);
    
    std::vector<int32_t> expected = {10, 20, 30, 40, 50};
    size_t index = 0;
    while (cass_iterator_next(iter)) {
      ASSERT_LT(index, expected.size());
      const CassValue* elem = cass_iterator_get_value(iter);
      int32_t val;
      ASSERT_EQ(cass_value_get_int32(elem, &val), CASS_OK);
      EXPECT_EQ(val, expected[index++]);
    }
    EXPECT_EQ(index, expected.size());
    cass_iterator_free(iter);
  }

  // Iterate v_float
  {
    const CassValue* value = cass_row_get_column_by_name(row.get(), "v_float");
    ASSERT_NE(value, nullptr);
    ASSERT_FALSE(cass_value_is_null(value));
    CassIterator* iter = cass_iterator_from_vector(value);
    ASSERT_NE(iter, nullptr);
    
    std::vector<float> expected = {1.1f, 2.2f, 3.3f, 4.4f};
    size_t index = 0;
    while (cass_iterator_next(iter)) {
      ASSERT_LT(index, expected.size());
      const CassValue* elem = cass_iterator_get_value(iter);
      float val;
      ASSERT_EQ(cass_value_get_float(elem, &val), CASS_OK);
      EXPECT_NEAR(val, expected[index++], 0.001f);
    }
    EXPECT_EQ(index, expected.size());
    cass_iterator_free(iter);
  }

  // Iterate v_text
  {
    const CassValue* value = cass_row_get_column_by_name(row.get(), "v_text");
    ASSERT_NE(value, nullptr);
    ASSERT_FALSE(cass_value_is_null(value));
    CassIterator* iter = cass_iterator_from_vector(value);
    ASSERT_NE(iter, nullptr);
    
    std::vector<std::string> expected = {"hello", "world", "test"};
    size_t index = 0;
    while (cass_iterator_next(iter)) {
      ASSERT_LT(index, expected.size());
      const CassValue* elem = cass_iterator_get_value(iter);
      const char* str;
      size_t str_len;
      ASSERT_EQ(cass_value_get_string(elem, &str, &str_len), CASS_OK);
      EXPECT_EQ(std::string(str, str_len), expected[index++]);
    }
    EXPECT_EQ(index, expected.size());
    cass_iterator_free(iter);
  }

  // Iterate v_boolean
  {
    const CassValue* value = cass_row_get_column_by_name(row.get(), "v_boolean");
    ASSERT_NE(value, nullptr);
    ASSERT_FALSE(cass_value_is_null(value));
    CassIterator* iter = cass_iterator_from_vector(value);
    ASSERT_NE(iter, nullptr);
    
    std::vector<cass_bool_t> expected = {cass_true, cass_false, cass_true};
    size_t index = 0;
    while (cass_iterator_next(iter)) {
      ASSERT_LT(index, expected.size());
      const CassValue* elem = cass_iterator_get_value(iter);
      cass_bool_t val;
      ASSERT_EQ(cass_value_get_bool(elem, &val), CASS_OK);
      EXPECT_EQ(val, expected[index++]);
    }
    EXPECT_EQ(index, expected.size());
    cass_iterator_free(iter);
  }

  // Iterate v_tinyint
  {
    const CassValue* value = cass_row_get_column_by_name(row.get(), "v_tinyint");
    ASSERT_NE(value, nullptr);
    ASSERT_FALSE(cass_value_is_null(value));
    CassIterator* iter = cass_iterator_from_vector(value);
    ASSERT_NE(iter, nullptr);
    
    std::vector<int8_t> expected = {1, 2, 3, 4, 5, 6, 7};
    size_t index = 0;
    while (cass_iterator_next(iter)) {
      ASSERT_LT(index, expected.size());
      const CassValue* elem = cass_iterator_get_value(iter);
      int8_t val;
      ASSERT_EQ(cass_value_get_int8(elem, &val), CASS_OK);
      EXPECT_EQ(val, expected[index++]);
    }
    EXPECT_EQ(index, expected.size());
    cass_iterator_free(iter);
  }
}

/**
 * Test vector dimension limits
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Dimension limits are enforced correctly
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTests, DimensionLimits) {
  CHECK_FAILURE;
  CHECK_VERSION("5.0.0");
  
  // Test minimum dimension (1)
  session_.execute("CREATE TABLE dim_test1 (id int PRIMARY KEY, v vector<int, 1>)");
  
  Prepared prep1 = session_.prepare("INSERT INTO dim_test1 (id, v) VALUES (?, ?)");
  Statement stmt1 = prep1.bind();
  cass_statement_bind_int32(stmt1.get(), 0, 1);
  
  const CassDataType* type1 = cass_prepared_parameter_data_type(prep1.get(), 1);
  CassVector* vec1 = cass_vector_new_from_data_type(type1);
  ASSERT_NE(vec1, nullptr);
  ASSERT_EQ(cass_vector_append_int32(vec1, 42), CASS_OK);
  // Second element should fail
  ASSERT_EQ(cass_vector_append_int32(vec1, 43), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  cass_statement_bind_vector(stmt1.get(), 1, vec1);
  cass_vector_free(vec1);
  session_.execute(stmt1);
  
  // Test embeddings-size dimension (1536)
  session_.execute("CREATE TABLE dim_test2 (id int PRIMARY KEY, v vector<float, 1536>)");
  
  Prepared prep2 = session_.prepare("INSERT INTO dim_test2 (id, v) VALUES (?, ?)");
  Statement stmt2 = prep2.bind();
  cass_statement_bind_int32(stmt2.get(), 0, 1);
  
  const CassDataType* type2 = cass_prepared_parameter_data_type(prep2.get(), 1);
  CassVector* vec2 = cass_vector_new_from_data_type(type2);
  ASSERT_NE(vec2, nullptr);
  
  // Add exactly 1536 elements
  for (int i = 0; i < 1536; i++) {
    ASSERT_EQ(cass_vector_append_float(vec2, static_cast<float>(i) / 100.0f), CASS_OK);
  }
  // 1537th element should fail
  ASSERT_EQ(cass_vector_append_float(vec2, 0.0f), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  
  cass_statement_bind_vector(stmt2.get(), 1, vec2);
  cass_vector_free(vec2);
  session_.execute(stmt2);
  
  // Verify the 1536-element vector
  Result result = session_.execute("SELECT v FROM dim_test2 WHERE id = 1");
  ASSERT_EQ(result.row_count(), 1u);
  Row row = result.first_row();
  const CassValue* value = cass_row_get_column_by_name(row.get(), "v");
  
  CassIterator* iter = cass_iterator_from_vector(value);
  ASSERT_NE(iter, nullptr);
  
  int count = 0;
  while (cass_iterator_next(iter)) {
    count++;
  }
  cass_iterator_free(iter);
  EXPECT_EQ(count, 1536);
  
  // Test invalid dimensions
  CassVector* vec_invalid = cass_vector_new(CASS_VALUE_TYPE_INT, 0);
  EXPECT_EQ(vec_invalid, nullptr);
  
  vec_invalid = cass_vector_new(CASS_VALUE_TYPE_INT, 10000);  // > 8192
  EXPECT_EQ(vec_invalid, nullptr);
}

/**
 * Test error cases for vector operations
 *
 * @test_category data_types:vector
 * @test_category error
 * @cassandra_version 5.0.0
 * @expected_result Error cases are handled correctly
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTests, ErrorCases) {
  CHECK_FAILURE;
  CHECK_VERSION("5.0.0");
  
  session_.execute("CREATE TABLE error_test (id int PRIMARY KEY, v vector<int, 3>)");
  
  // Test type mismatch
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_INT, 3);
    ASSERT_NE(vec, nullptr);
    // Try to append wrong type
    EXPECT_EQ(cass_vector_append_string(vec, "not an int"), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
    EXPECT_EQ(cass_vector_append_float(vec, 3.14f), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
    cass_vector_free(vec);
  }
  
  // Test dimension overflow
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
    ASSERT_NE(vec, nullptr);
    ASSERT_EQ(cass_vector_append_int32(vec, 1), CASS_OK);
    ASSERT_EQ(cass_vector_append_int32(vec, 2), CASS_OK);
    // Third element should fail
    EXPECT_EQ(cass_vector_append_int32(vec, 3), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
    cass_vector_free(vec);
  }
  
  // Test incomplete vector (not enough elements)
  {
    Prepared prep = session_.prepare("INSERT INTO error_test (id, v) VALUES (?, ?)");
    Statement stmt = prep.bind();
    cass_statement_bind_int32(stmt.get(), 0, 2);
    
    const CassDataType* type = cass_prepared_parameter_data_type(prep.get(), 1);
    CassVector* vec = cass_vector_new_from_data_type(type);
    ASSERT_NE(vec, nullptr);
    // Add only 2 elements when 3 are required
    ASSERT_EQ(cass_vector_append_int32(vec, 1), CASS_OK);
    ASSERT_EQ(cass_vector_append_int32(vec, 2), CASS_OK);
    cass_statement_bind_vector(stmt.get(), 1, vec);
    cass_vector_free(vec);
    
    // This should fail at the server
    Result result = session_.execute(stmt, false);
    EXPECT_FALSE(result);
    // Cassandra 5.0.5 error message format
    EXPECT_TRUE(contains(result.error_message(), "Not enough bytes to read a vector"));
  }
}