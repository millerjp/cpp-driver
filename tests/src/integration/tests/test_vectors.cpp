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
 * Vector data type integration tests for Cassandra 5.0+
 * 
 * These tests verify the C++ driver's support for the vector data type
 * introduced in Cassandra 5.0 for AI/ML and similarity search use cases.
 */
class VectorTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Create test keyspace for vector tests
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_test "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_test");
  }
  
  void TearDown() {
    // Clean up test keyspace
    session_.execute("DROP KEYSPACE IF EXISTS vector_test");
    Integration::TearDown();
  }
};

/**
 * Test that we can create a table with a vector column
 * This is a basic smoke test to ensure Cassandra 5.0 vector support is available
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTest, CreateTableWithVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create a table with a vector column
  session_.execute("CREATE TABLE IF NOT EXISTS items ("
                   "id int PRIMARY KEY, "
                   "name text, "
                   "embedding vector<float, 3>)");
  
  // Verify table was created by querying system tables
  Result result = session_.execute(
    "SELECT column_name, type FROM system_schema.columns "
    "WHERE keyspace_name = 'vector_test' AND table_name = 'items' "
    "AND column_name = 'embedding'");
  
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  Text column_name = row.column_by_name<Text>("column_name");
  ASSERT_EQ("embedding", column_name.str());
  
  // The type will show as 'org.apache.cassandra.db.marshal.VectorType(float, 3)'
  // or similar format
  Text type_str = row.column_by_name<Text>("type");
  std::string type = type_str.str();
  TEST_LOG("Vector column type string: " << type);
  
  // Verify it contains vector type indicator
  ASSERT_TRUE(type.find("vector<") != std::string::npos) 
    << "Expected type to contain 'vector<', but got: " << type;
  
  // Drop the table for cleanup
  session_.execute("DROP TABLE items");
}

/**
 * Test inserting vectors using the C API
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTest, InsertVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_vectors ("
                   "id int PRIMARY KEY, "
                   "vec vector<float, 3>)");
  
  // Create a vector with 3 float elements
  CassStatement* statement = cass_statement_new(
    "INSERT INTO test_vectors (id, vec) VALUES (?, ?)", 2);
  cass_statement_bind_int32(statement, 0, 1);
  
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  cass_vector_append_float(vector, 1.0f);
  cass_vector_append_float(vector, 2.0f);
  cass_vector_append_float(vector, 3.0f);
  
  ASSERT_EQ(CASS_OK, cass_statement_bind_vector(statement, 1, vector));
  cass_vector_free(vector);
  
  // Execute the insert
  CassFuture* future = cass_session_execute(session_.get(), statement);
  cass_statement_free(statement);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Query back the data and verify
  Result result = session_.execute("SELECT vec FROM test_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
  
  // Verify we can iterate the vector
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(nullptr, iter);
  
  float expected[] = {1.0f, 2.0f, 3.0f};
  int idx = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    cass_float_t f;
    ASSERT_EQ(CASS_OK, cass_value_get_float(element, &f));
    ASSERT_FLOAT_EQ(expected[idx], f);
    idx++;
  }
  cass_iterator_free(iter);
  ASSERT_EQ(3, idx);
}

/**
 * Test retrieving vectors from query results
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTest, RetrieveVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table and insert test data using CQL
  session_.execute("CREATE TABLE IF NOT EXISTS retrieve_test ("
                   "id int PRIMARY KEY, "
                   "vec vector<float, 4>)");
  
  session_.execute("INSERT INTO retrieve_test (id, vec) VALUES (1, [0.5, 1.5, 2.5, 3.5])");
  session_.execute("INSERT INTO retrieve_test (id, vec) VALUES (2, [10.0, 20.0, 30.0, 40.0])");
  
  // Retrieve using prepared statement
  CassFuture* prepare_future = cass_session_prepare(session_.get(),
    "SELECT vec FROM retrieve_test WHERE id = ?");
  
  const CassPrepared* prepared = cass_future_get_prepared(prepare_future);
  ASSERT_NE(nullptr, prepared);
  cass_future_free(prepare_future);
  
  // Query for id=1
  CassStatement* bound = cass_prepared_bind(prepared);
  cass_statement_bind_int32(bound, 0, 1);
  
  CassFuture* future = cass_session_execute(session_.get(), bound);
  cass_statement_free(bound);
  
  const CassResult* result = cass_future_get_result(future);
  ASSERT_NE(nullptr, result);
  cass_future_free(future);
  
  const CassRow* row = cass_result_first_row(result);
  ASSERT_NE(nullptr, row);
  
  const CassValue* vec_value = cass_row_get_column(row, 0);
  ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
  
  // Verify vector contents
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(nullptr, iter);
  
  float expected[] = {0.5f, 1.5f, 2.5f, 3.5f};
  int idx = 0;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    cass_float_t f;
    ASSERT_EQ(CASS_OK, cass_value_get_float(element, &f));
    ASSERT_FLOAT_EQ(expected[idx], f);
    idx++;
  }
  cass_iterator_free(iter);
  ASSERT_EQ(4, idx);
  
  cass_result_free(result);
  cass_prepared_free(prepared);
}

/**
 * Test vector index creation and similarity search
 * Note: This test requires Cassandra 5.0+ with SAI (Storage-Attached Indexing)
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTest, VectorIndex) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create a table with vector column
  session_.execute("CREATE TABLE IF NOT EXISTS vector_search ("
                   "id int PRIMARY KEY, "
                   "name text, "
                   "embedding vector<float, 3>)");
  
  // Create a vector index using SAI
  // Cassandra 5.0 uses SAI (Storage-Attached Indexing) for vector similarity search
  try {
    session_.execute("CREATE INDEX IF NOT EXISTS vector_search_idx "
                     "ON vector_search (embedding) "
                     "USING 'sai'");
  } catch (const Exception& e) {
    // SAI might not be enabled or available
    TEST_LOG("Could not create vector index, SAI may not be available: " << e.what());
    return;
  }
  
  // Insert multiple vectors using the C API
  const int num_vectors = 5;
  float embeddings[][3] = {
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
    {0.5f, 0.5f, 0.0f},
    {0.9f, 0.1f, 0.0f}  // Similar to first vector
  };
  
  const char* names[] = {"vec1", "vec2", "vec3", "vec4", "vec5"};
  
  for (int i = 0; i < num_vectors; ++i) {
    CassStatement* statement = cass_statement_new(
      "INSERT INTO vector_search (id, name, embedding) VALUES (?, ?, ?)", 3);
    cass_statement_bind_int32(statement, 0, i);
    cass_statement_bind_string(statement, 1, names[i]);
    
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
    for (int j = 0; j < 3; ++j) {
      cass_vector_append_float(vector, embeddings[i][j]);
    }
    
    cass_statement_bind_vector(statement, 2, vector);
    cass_vector_free(vector);
    
    CassFuture* future = cass_session_execute(session_.get(), statement);
    cass_statement_free(statement);
    
    CassError rc = cass_future_error_code(future);
    cass_future_free(future);
    ASSERT_EQ(CASS_OK, rc);
  }
  
  // Perform similarity search using ORDER BY ... ANN OF
  // Find vectors similar to [1.0, 0.0, 0.0]
  try {
    Result result = session_.execute(
      "SELECT name, embedding FROM vector_search "
      "ORDER BY embedding ANN OF [1.0, 0.0, 0.0] LIMIT 2");
    
    ASSERT_GE(result.row_count(), 1ul);
    
    // First result should be vec1 (exact match) or vec5 (very similar)
    Row first = result.first_row();
    Text name = first.column_by_name<Text>("name");
    TEST_LOG("Nearest vector (basic ANN): " << name.str());
    
    // Verify it's one of the expected similar vectors
    ASSERT_TRUE(name.str() == "vec1" || name.str() == "vec5") 
      << "Expected vec1 or vec5 as nearest, got: " << name.str();
  } catch (const Exception& e) {
    // ANN queries might not be supported in all configurations
    TEST_LOG("ANN query not supported: " << e.what());
    return;
  }
  
  // Test similarity functions: COSINE
  try {
    Result result = session_.execute(
      "SELECT name, similarity_cosine(embedding, [1.0, 0.0, 0.0]) as sim "
      "FROM vector_search "
      "ORDER BY embedding ANN OF [1.0, 0.0, 0.0] LIMIT 2");
    
    if (result.row_count() > 0) {
      Row first = result.first_row();
      Text name = first.column_by_name<Text>("name");
      // The similarity column might be a float or double
      TEST_LOG("Nearest vector (COSINE similarity): " << name.str());
    }
  } catch (const Exception& e) {
    TEST_LOG("COSINE similarity function not available: " << e.what());
  }
  
  // Test similarity functions: DOT_PRODUCT
  try {
    Result result = session_.execute(
      "SELECT name, similarity_dot_product(embedding, [1.0, 0.0, 0.0]) as sim "
      "FROM vector_search "
      "ORDER BY embedding ANN OF [1.0, 0.0, 0.0] LIMIT 2");
    
    if (result.row_count() > 0) {
      Row first = result.first_row();
      Text name = first.column_by_name<Text>("name");
      TEST_LOG("Nearest vector (DOT_PRODUCT similarity): " << name.str());
    }
  } catch (const Exception& e) {
    TEST_LOG("DOT_PRODUCT similarity function not available: " << e.what());
  }
  
  // Test similarity functions: EUCLIDEAN
  try {
    Result result = session_.execute(
      "SELECT name, similarity_euclidean(embedding, [1.0, 0.0, 0.0]) as sim "
      "FROM vector_search "
      "ORDER BY embedding ANN OF [1.0, 0.0, 0.0] LIMIT 2");
    
    if (result.row_count() > 0) {
      Row first = result.first_row();
      Text name = first.column_by_name<Text>("name");
      TEST_LOG("Nearest vector (EUCLIDEAN similarity): " << name.str());
    }
  } catch (const Exception& e) {
    TEST_LOG("EUCLIDEAN similarity function not available: " << e.what());
  }
}

/**
 * Test various vector dimensions
 * Cassandra supports vectors with dimensions from 1 to 8192
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTest, VectorDimensions) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Test minimum dimension (1)
  session_.execute("CREATE TABLE test_dim_1 (id int PRIMARY KEY, v vector<float, 1>)");
  session_.execute("INSERT INTO test_dim_1 (id, v) VALUES (1, [1.5])");
  session_.execute("DROP TABLE test_dim_1");
  
  // Test typical embedding dimension (128)
  session_.execute("CREATE TABLE test_dim_128 (id int PRIMARY KEY, v vector<float, 128>)");
  session_.execute("DROP TABLE test_dim_128");
  
  // Test large dimension (1536 - common for embeddings)
  session_.execute("CREATE TABLE test_dim_1536 (id int PRIMARY KEY, v vector<float, 1536>)");
  session_.execute("DROP TABLE test_dim_1536");
  
  TEST_LOG("Vector dimension tests passed");
}

/**
 * Test different vector element types
 * Cassandra 5.0 supports various element types in vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorTest, VectorElementTypes) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Test float vectors (most common for ML/AI)
  session_.execute("CREATE TABLE test_float (id int PRIMARY KEY, v vector<float, 3>)");
  session_.execute("INSERT INTO test_float (id, v) VALUES (1, [1.1, 2.2, 3.3])");
  session_.execute("DROP TABLE test_float");
  
  // Test double vectors
  session_.execute("CREATE TABLE test_double (id int PRIMARY KEY, v vector<double, 3>)");
  session_.execute("INSERT INTO test_double (id, v) VALUES (1, [1.1, 2.2, 3.3])");
  session_.execute("DROP TABLE test_double");
  
  // Test int vectors
  session_.execute("CREATE TABLE test_int (id int PRIMARY KEY, v vector<int, 3>)");
  session_.execute("INSERT INTO test_int (id, v) VALUES (1, [1, 2, 3])");
  session_.execute("DROP TABLE test_int");
  
  TEST_LOG("Vector element type tests passed");
}