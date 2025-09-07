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
 * Simple vector integration test for Cassandra 5.0+
 */
class VectorSimpleTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Create test keyspace
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_test "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_test");
  }
  
  void TearDown() {
    // Clean up
    session_.execute("DROP KEYSPACE IF EXISTS vector_test");
    Integration::TearDown();
  }
};

/**
 * Test basic float vector insert and select with round-trip verification
 */
CASSANDRA_INTEGRATION_TEST_F(VectorSimpleTest, SimpleFloatVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table with vector column
  session_.execute("CREATE TABLE IF NOT EXISTS test_vectors ("
                   "id int PRIMARY KEY, "
                   "embedding vector<float, 3>)");
  
  // Create a vector using C API
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  ASSERT_NE(vector, nullptr);
  
  // Add three float values
  ASSERT_EQ(cass_vector_append_float(vector, 1.0f), CASS_OK);
  ASSERT_EQ(cass_vector_append_float(vector, 2.0f), CASS_OK);
  ASSERT_EQ(cass_vector_append_float(vector, 3.0f), CASS_OK);
  
  // Use prepared statement for proper parameter binding
  Prepared prepared = session_.prepare("INSERT INTO test_vectors (id, embedding) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  // Get the raw statement pointer to use C API binding
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  // Execute the insert
  session_.execute(stmt);
  
  // Free the vector
  cass_vector_free(vector);
  
  // Query the data back
  Result result = session_.execute("SELECT id, embedding FROM test_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  ASSERT_EQ(1, row.column_by_name<Integer>("id").value());
  
  // Get the vector value and iterate through elements
  const CassValue* vec_value = cass_row_get_column_by_name(row.get(), "embedding");
  ASSERT_NE(vec_value, nullptr);
  
  // Debug: Check if the value is null
  if (cass_value_is_null(vec_value)) {
    TEST_LOG("WARNING: Vector value is NULL");
  }
  
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(iter, nullptr) << "Failed to create iterator from vector value";
  
  // Read and verify the vector elements
  std::vector<float> values;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    float val;
    ASSERT_EQ(cass_value_get_float(element, &val), CASS_OK);
    values.push_back(val);
  }
  cass_iterator_free(iter);
  
  // Verify we got the right values back
  ASSERT_EQ(values.size(), 3ul);
  ASSERT_FLOAT_EQ(values[0], 1.0f);
  ASSERT_FLOAT_EQ(values[1], 2.0f);
  ASSERT_FLOAT_EQ(values[2], 3.0f);
  
  TEST_LOG("Successfully completed float vector round-trip test!");
}

/**
 * Test multiple vector inserts
 */
CASSANDRA_INTEGRATION_TEST_F(VectorSimpleTest, MultipleVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS multi_vectors ("
                   "id int PRIMARY KEY, "
                   "vec vector<float, 2>)");
  
  // Prepare statement once
  Prepared prepared = session_.prepare("INSERT INTO multi_vectors (id, vec) VALUES (?, ?)");
  
  // Insert multiple vectors
  for (int i = 1; i <= 3; i++) {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 2);
    cass_vector_append_float(vec, static_cast<float>(i));
    cass_vector_append_float(vec, static_cast<float>(i * 10));
    
    Statement stmt = prepared.bind();
    stmt.bind<Integer>(0, Integer(i));
    cass_statement_bind_vector(stmt.get(), 1, vec);
    
    session_.execute(stmt);
    cass_vector_free(vec);
  }
  
  // Query all back
  Result result = session_.execute("SELECT * FROM multi_vectors");
  ASSERT_EQ(3ul, result.row_count());
  
  TEST_LOG("Successfully inserted 3 vectors!");
}

/**
 * Test integer vector
 */
CASSANDRA_INTEGRATION_TEST_F(VectorSimpleTest, IntegerVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table with int vector
  session_.execute("CREATE TABLE IF NOT EXISTS int_vectors ("
                   "id int PRIMARY KEY, "
                   "numbers vector<int, 4>)");
  
  CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_INT, 4);
  ASSERT_NE(vec, nullptr);
  
  // Add four integers
  for (int i = 1; i <= 4; i++) {
    ASSERT_EQ(cass_vector_append_int32(vec, i * 100), CASS_OK);
  }
  
  Prepared prepared = session_.prepare("INSERT INTO int_vectors (id, numbers) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  cass_statement_bind_vector(stmt.get(), 1, vec);
  
  session_.execute(stmt);
  cass_vector_free(vec);
  
  // Verify insert succeeded
  Result result = session_.execute("SELECT * FROM int_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  TEST_LOG("Successfully inserted integer vector!");
}

/**
 * Test text vector with round-trip (variable-length elements with UVINT)
 */
CASSANDRA_INTEGRATION_TEST_F(VectorSimpleTest, TextVectorRoundTrip) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table with text vector
  session_.execute("CREATE TABLE IF NOT EXISTS text_vectors ("
                   "id int PRIMARY KEY, "
                   "tags vector<text, 3>)");
  
  // Create vector
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TEXT, 3);
  ASSERT_NE(vector, nullptr);
  
  // Add text values
  ASSERT_EQ(cass_vector_append_string(vector, "hello"), CASS_OK);
  ASSERT_EQ(cass_vector_append_string(vector, "world"), CASS_OK);
  ASSERT_EQ(cass_vector_append_string(vector, "test"), CASS_OK);
  
  // Insert using prepared statement
  Prepared prepared = session_.prepare("INSERT INTO text_vectors (id, tags) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  session_.execute(stmt);
  cass_vector_free(vector);
  
  // Query back and verify
  Result result = session_.execute("SELECT tags FROM text_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(iter, nullptr);
  
  std::vector<std::string> values;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    const char* str;
    size_t str_len;
    ASSERT_EQ(cass_value_get_string(element, &str, &str_len), CASS_OK);
    values.push_back(std::string(str, str_len));
  }
  cass_iterator_free(iter);
  
  ASSERT_EQ(values.size(), 3ul);
  ASSERT_EQ(values[0], "hello");
  ASSERT_EQ(values[1], "world");
  ASSERT_EQ(values[2], "test");
  
  TEST_LOG("Successfully completed text vector round-trip test with UVINT encoding!");
}