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
#include <set>

/**
 * Batch statement tests with vectors
 */
class VectorBatchTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
  }
};

/**
 * Test batch insert with multiple vector types
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Batch statements successfully insert multiple vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorBatchTest, BatchInsertVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create keyspace and use it
  session_.execute("CREATE KEYSPACE IF NOT EXISTS test_batch_vectors "
                   "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
  session_.execute("USE test_batch_vectors");
  
  // Create table with multiple vector columns
  session_.execute("CREATE TABLE IF NOT EXISTS batch_vectors ("
                   "id int PRIMARY KEY, "
                   "float_vec vector<float, 3>, "
                   "int_vec vector<int, 2>, "
                   "text_vec vector<text, 2>)");
  
  // Create batch statement
  CassBatch* batch = cass_batch_new(CASS_BATCH_TYPE_LOGGED);
  
  // Add first insert
  {
    const char* query = "INSERT INTO batch_vectors (id, float_vec, int_vec, text_vec) VALUES (?, ?, ?, ?)";
    CassStatement* stmt = cass_statement_new(query, 4);
    
    cass_statement_bind_int32(stmt, 0, 1);
    
    // Bind float vector
    CassVector* float_vec = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
    cass_vector_append_float(float_vec, 1.0f);
    cass_vector_append_float(float_vec, 2.0f);
    cass_vector_append_float(float_vec, 3.0f);
    cass_statement_bind_vector(stmt, 1, float_vec);
    cass_vector_free(float_vec);
    
    // Bind int vector
    CassVector* int_vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
    cass_vector_append_int32(int_vec, 10);
    cass_vector_append_int32(int_vec, 20);
    cass_statement_bind_vector(stmt, 2, int_vec);
    cass_vector_free(int_vec);
    
    // Bind text vector
    CassVector* text_vec = cass_vector_new(CASS_VALUE_TYPE_TEXT, 2);
    cass_vector_append_string(text_vec, "hello");
    cass_vector_append_string(text_vec, "world");
    cass_statement_bind_vector(stmt, 3, text_vec);
    cass_vector_free(text_vec);
    
    cass_batch_add_statement(batch, stmt);
    cass_statement_free(stmt);
  }
  
  // Add second insert
  {
    const char* query = "INSERT INTO batch_vectors (id, float_vec, int_vec, text_vec) VALUES (?, ?, ?, ?)";
    CassStatement* stmt = cass_statement_new(query, 4);
    
    cass_statement_bind_int32(stmt, 0, 2);
    
    // Bind float vector
    CassVector* float_vec = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
    cass_vector_append_float(float_vec, 4.0f);
    cass_vector_append_float(float_vec, 5.0f);
    cass_vector_append_float(float_vec, 6.0f);
    cass_statement_bind_vector(stmt, 1, float_vec);
    cass_vector_free(float_vec);
    
    // Bind int vector
    CassVector* int_vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
    cass_vector_append_int32(int_vec, 30);
    cass_vector_append_int32(int_vec, 40);
    cass_statement_bind_vector(stmt, 2, int_vec);
    cass_vector_free(int_vec);
    
    // Bind text vector
    CassVector* text_vec = cass_vector_new(CASS_VALUE_TYPE_TEXT, 2);
    cass_vector_append_string(text_vec, "foo");
    cass_vector_append_string(text_vec, "bar");
    cass_statement_bind_vector(stmt, 3, text_vec);
    cass_vector_free(text_vec);
    
    cass_batch_add_statement(batch, stmt);
    cass_statement_free(stmt);
  }
  
  // Execute batch
  CassFuture* future = cass_session_execute_batch(session_.get(), batch);
  cass_batch_free(batch);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify data was inserted
  Result result = session_.execute("SELECT * FROM batch_vectors");
  ASSERT_EQ(2ul, result.row_count());
  
  // Verify both rows exist (order not guaranteed)
  Rows rows = result.rows();
  std::set<int> ids;
  for (size_t i = 0; i < result.row_count(); ++i) {
    Row row = rows.next();
    ids.insert(row.column_by_name<Integer>("id").value());
  }
  ASSERT_TRUE(ids.count(1) > 0);
  ASSERT_TRUE(ids.count(2) > 0);
}

/**
 * Test batch with prepared statements containing vectors
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Batch statements work with prepared statements
 */
CASSANDRA_INTEGRATION_TEST_F(VectorBatchTest, BatchPreparedVectors) {
  CHECK_FAILURE;
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS batch_prepared ("
                   "id int PRIMARY KEY, "
                   "data vector<double, 4>)");
  
  // Prepare statement
  Prepared prepared = session_.prepare("INSERT INTO batch_prepared (id, data) VALUES (?, ?)");
  
  // Create batch
  CassBatch* batch = cass_batch_new(CASS_BATCH_TYPE_LOGGED);
  
  // Add multiple prepared statements to batch
  for (int i = 1; i <= 3; ++i) {
    CassStatement* stmt = cass_prepared_bind(prepared.get());
    cass_statement_bind_int32(stmt, 0, i);
    
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_DOUBLE, 4);
    cass_vector_append_double(vec, i * 1.1);
    cass_vector_append_double(vec, i * 2.2);
    cass_vector_append_double(vec, i * 3.3);
    cass_vector_append_double(vec, i * 4.4);
    cass_statement_bind_vector(stmt, 1, vec);
    cass_vector_free(vec);
    
    cass_batch_add_statement(batch, stmt);
    cass_statement_free(stmt);
  }
  
  // Execute batch
  CassFuture* future = cass_session_execute_batch(session_.get(), batch);
  cass_batch_free(batch);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify all rows inserted
  Result result = session_.execute("SELECT COUNT(*) FROM batch_prepared");
  ASSERT_EQ(3, result.first_row().column_by_name<BigInteger>("count").value());
}

/**
 * Test mixed batch with vectors and regular types
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Batch statements work with mixed types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorBatchTest, BatchMixedTypes) {
  CHECK_FAILURE;
  
  // Create two tables
  session_.execute("CREATE TABLE IF NOT EXISTS table_with_vector ("
                   "id int PRIMARY KEY, "
                   "vec vector<float, 2>)");
  
  session_.execute("CREATE TABLE IF NOT EXISTS table_without_vector ("
                   "id int PRIMARY KEY, "
                   "value text)");
  
  // Create batch with mixed operations
  CassBatch* batch = cass_batch_new(CASS_BATCH_TYPE_LOGGED);
  
  // Add vector insert
  {
    CassStatement* stmt = cass_statement_new(
        "INSERT INTO table_with_vector (id, vec) VALUES (?, ?)", 2);
    cass_statement_bind_int32(stmt, 0, 1);
    
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 2);
    cass_vector_append_float(vec, 1.5f);
    cass_vector_append_float(vec, 2.5f);
    cass_statement_bind_vector(stmt, 1, vec);
    cass_vector_free(vec);
    
    cass_batch_add_statement(batch, stmt);
    cass_statement_free(stmt);
  }
  
  // Add regular insert
  {
    CassStatement* stmt = cass_statement_new(
        "INSERT INTO table_without_vector (id, value) VALUES (?, ?)", 2);
    cass_statement_bind_int32(stmt, 0, 1);
    cass_statement_bind_string(stmt, 1, "test value");
    cass_batch_add_statement(batch, stmt);
    cass_statement_free(stmt);
  }
  
  // Execute batch
  CassFuture* future = cass_session_execute_batch(session_.get(), batch);
  cass_batch_free(batch);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify both inserts succeeded
  Result r1 = session_.execute("SELECT * FROM table_with_vector WHERE id = 1");
  ASSERT_EQ(1ul, r1.row_count());
  
  Result r2 = session_.execute("SELECT * FROM table_without_vector WHERE id = 1");
  ASSERT_EQ(1ul, r2.row_count());
  ASSERT_EQ(Text("test value"), r2.first_row().column_by_name<Text>("value"));
}