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
 * Simple statement vector tests (not prepared statements)
 */
class VectorSimpleStatementTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Create test keyspace
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_simple_stmt "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_simple_stmt");
  }
  
  void TearDown() {
    session_.execute("DROP KEYSPACE IF EXISTS vector_simple_stmt");
    Integration::TearDown();
  }
};

/**
 * Test simple statement with float vector
 */
CASSANDRA_INTEGRATION_TEST_F(VectorSimpleStatementTest, SimpleStatementFloatVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_float ("
                   "id int PRIMARY KEY, "
                   "vec vector<float, 3>)");
  
  // Create vector
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  ASSERT_NE(vector, nullptr);
  
  ASSERT_EQ(cass_vector_append_float(vector, 1.5f), CASS_OK);
  ASSERT_EQ(cass_vector_append_float(vector, 2.5f), CASS_OK);
  ASSERT_EQ(cass_vector_append_float(vector, 3.5f), CASS_OK);
  
  // Create SIMPLE statement (not prepared)
  CassStatement* stmt = cass_statement_new("INSERT INTO test_float (id, vec) VALUES (?, ?)", 2);
  ASSERT_NE(stmt, nullptr);
  
  // Bind parameters
  ASSERT_EQ(cass_statement_bind_int32(stmt, 0, 1), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(stmt, 1, vector), CASS_OK);
  
  // Execute
  CassFuture* future = cass_session_execute(session_.get(), stmt);
  CassError rc = cass_future_error_code(future);
  
  if (rc != CASS_OK) {
    // Simple statement test failed - error details would be in test output
    TEST_LOG("Error executing simple statement with vector");
  }
  
  ASSERT_EQ(rc, CASS_OK);
  
  cass_future_free(future);
  cass_statement_free(stmt);
  cass_vector_free(vector);
  
  // Verify data was inserted correctly
  Result result = session_.execute("SELECT vec FROM test_float WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(iter, nullptr);
  
  float expected[] = {1.5f, 2.5f, 3.5f};
  int idx = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    float val;
    ASSERT_EQ(cass_value_get_float(element, &val), CASS_OK);
    EXPECT_FLOAT_EQ(val, expected[idx]) << "Mismatch at index " << idx;
    idx++;
  }
  EXPECT_EQ(idx, 3);
  
  cass_iterator_free(iter);
  
  TEST_LOG("Simple statement with float vector successful!");
}

/**
 * Test simple statement with text vector
 */
CASSANDRA_INTEGRATION_TEST_F(VectorSimpleStatementTest, SimpleStatementTextVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_text ("
                   "id int PRIMARY KEY, "
                   "vec vector<text, 2>)");
  
  // Create vector
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TEXT, 2);
  ASSERT_NE(vector, nullptr);
  
  ASSERT_EQ(cass_vector_append_string(vector, "hello"), CASS_OK);
  ASSERT_EQ(cass_vector_append_string(vector, "world"), CASS_OK);
  
  // Create SIMPLE statement
  CassStatement* stmt = cass_statement_new("INSERT INTO test_text (id, vec) VALUES (?, ?)", 2);
  ASSERT_NE(stmt, nullptr);
  
  ASSERT_EQ(cass_statement_bind_int32(stmt, 0, 2), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector(stmt, 1, vector), CASS_OK);
  
  // Execute
  CassFuture* future = cass_session_execute(session_.get(), stmt);
  ASSERT_EQ(cass_future_error_code(future), CASS_OK);
  
  cass_future_free(future);
  cass_statement_free(stmt);
  cass_vector_free(vector);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM test_text WHERE id = 2");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(iter, nullptr);
  
  const char* expected[] = {"hello", "world"};
  int idx = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    const char* str;
    size_t len;
    ASSERT_EQ(cass_value_get_string(element, &str, &len), CASS_OK);
    EXPECT_EQ(std::string(str, len), expected[idx]) << "Mismatch at index " << idx;
    idx++;
  }
  EXPECT_EQ(idx, 2);
  
  cass_iterator_free(iter);
  
  TEST_LOG("Simple statement with text vector successful!");
}

/**
 * Test simple statement by name binding
 */
CASSANDRA_INTEGRATION_TEST_F(VectorSimpleStatementTest, SimpleStatementNamedBinding) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_named ("
                   "id int PRIMARY KEY, "
                   "vec vector<int, 4>)");
  
  // Create vector
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_INT, 4);
  ASSERT_NE(vector, nullptr);
  
  ASSERT_EQ(cass_vector_append_int32(vector, 10), CASS_OK);
  ASSERT_EQ(cass_vector_append_int32(vector, 20), CASS_OK);
  ASSERT_EQ(cass_vector_append_int32(vector, 30), CASS_OK);
  ASSERT_EQ(cass_vector_append_int32(vector, 40), CASS_OK);
  
  // Create statement with named parameters
  CassStatement* stmt = cass_statement_new("INSERT INTO test_named (id, vec) VALUES (:id, :vec)", 2);
  ASSERT_NE(stmt, nullptr);
  
  // Bind by name
  ASSERT_EQ(cass_statement_bind_int32_by_name(stmt, "id", 3), CASS_OK);
  ASSERT_EQ(cass_statement_bind_vector_by_name(stmt, "vec", vector), CASS_OK);
  
  // Execute
  CassFuture* future = cass_session_execute(session_.get(), stmt);
  ASSERT_EQ(cass_future_error_code(future), CASS_OK);
  
  cass_future_free(future);
  cass_statement_free(stmt);
  cass_vector_free(vector);
  
  // Verify
  Result result = session_.execute("SELECT vec FROM test_named WHERE id = 3");
  ASSERT_EQ(1ul, result.row_count());
  
  TEST_LOG("Simple statement with named binding successful!");
}

/**
 * Test batch statement with vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorSimpleStatementTest, BatchStatementWithVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_batch ("
                   "id int PRIMARY KEY, "
                   "vec vector<float, 2>)");
  
  // Create batch
  CassBatch* batch = cass_batch_new(CASS_BATCH_TYPE_LOGGED);
  ASSERT_NE(batch, nullptr);
  
  // Add multiple statements with vectors
  for (int i = 1; i <= 3; i++) {
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 2);
    ASSERT_NE(vector, nullptr);
    
    ASSERT_EQ(cass_vector_append_float(vector, i * 1.0f), CASS_OK);
    ASSERT_EQ(cass_vector_append_float(vector, i * 2.0f), CASS_OK);
    
    CassStatement* stmt = cass_statement_new("INSERT INTO test_batch (id, vec) VALUES (?, ?)", 2);
    ASSERT_EQ(cass_statement_bind_int32(stmt, 0, i), CASS_OK);
    ASSERT_EQ(cass_statement_bind_vector(stmt, 1, vector), CASS_OK);
    
    ASSERT_EQ(cass_batch_add_statement(batch, stmt), CASS_OK);
    
    cass_statement_free(stmt);
    cass_vector_free(vector);
  }
  
  // Execute batch
  CassFuture* future = cass_session_execute_batch(session_.get(), batch);
  ASSERT_EQ(cass_future_error_code(future), CASS_OK);
  
  cass_future_free(future);
  cass_batch_free(batch);
  
  // Verify batch was successful - just check we can query the table
  Result result = session_.execute("SELECT * FROM test_batch");
  EXPECT_EQ(result.row_count(), 3ul);
  
  TEST_LOG("Batch statement with vectors successful!");
}