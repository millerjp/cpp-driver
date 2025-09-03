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
 * Debug test to discover actual vector format from Cassandra
 */
class VectorDebugTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Enable error logging to see our debug messages
    cass_log_set_level(CASS_LOG_ERROR);
    
    // Create test keyspace
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_debug "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_debug");
  }
  
  void TearDown() {
    session_.execute("DROP KEYSPACE IF EXISTS vector_debug");
    Integration::TearDown();
  }
};

/**
 * Test various vector types to see the actual format
 */
CASSANDRA_INTEGRATION_TEST_F(VectorDebugTest, DiscoverVectorFormats) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  TEST_LOG("=== DISCOVERING VECTOR TYPE FORMATS ===");
  
  // Test 1: Float vector
  {
    TEST_LOG("Testing vector<float, 3>...");
    session_.execute("CREATE TABLE IF NOT EXISTS test_float (id int PRIMARY KEY, vec vector<float, 3>)");
    
    // Insert a simple vector
    session_.execute("INSERT INTO test_float (id, vec) VALUES (1, [1.0, 2.0, 3.0])");
    
    // Query back to trigger parsing
    Result result = session_.execute("SELECT vec FROM test_float WHERE id = 1");
    ASSERT_EQ(1ul, result.row_count());
    
    // Try to iterate - this will trigger our debug logging
    Row row = result.first_row();
    const CassValue* vec_value = cass_row_get_column(row.get(), 0);
    CassIterator* iter = cass_iterator_from_vector(vec_value);
    if (iter) {
      cass_iterator_free(iter);
    }
    
    session_.execute("DROP TABLE test_float");
  }
  
  // Test 2: Text vector
  {
    TEST_LOG("Testing vector<text, 2>...");
    session_.execute("CREATE TABLE IF NOT EXISTS test_text (id int PRIMARY KEY, vec vector<text, 2>)");
    
    // Insert a text vector
    session_.execute("INSERT INTO test_text (id, vec) VALUES (1, ['hello', 'world'])");
    
    // Query back
    Result result = session_.execute("SELECT vec FROM test_text WHERE id = 1");
    ASSERT_EQ(1ul, result.row_count());
    
    // Try to iterate
    Row row = result.first_row();
    const CassValue* vec_value = cass_row_get_column(row.get(), 0);
    CassIterator* iter = cass_iterator_from_vector(vec_value);
    if (iter) {
      cass_iterator_free(iter);
    }
    
    session_.execute("DROP TABLE test_text");
  }
  
  // Test 3: Blob vector
  {
    TEST_LOG("Testing vector<blob, 2>...");
    session_.execute("CREATE TABLE IF NOT EXISTS test_blob (id int PRIMARY KEY, vec vector<blob, 2>)");
    
    // Insert a blob vector
    session_.execute("INSERT INTO test_blob (id, vec) VALUES (1, [0x0102, 0x0304])");
    
    // Query back
    Result result = session_.execute("SELECT vec FROM test_blob WHERE id = 1");
    ASSERT_EQ(1ul, result.row_count());
    
    // Try to iterate
    Row row = result.first_row();
    const CassValue* vec_value = cass_row_get_column(row.get(), 0);
    CassIterator* iter = cass_iterator_from_vector(vec_value);
    if (iter) {
      cass_iterator_free(iter);
    }
    
    session_.execute("DROP TABLE test_blob");
  }
  
  // Test 4: Int vector
  {
    TEST_LOG("Testing vector<int, 4>...");
    session_.execute("CREATE TABLE IF NOT EXISTS test_int (id int PRIMARY KEY, vec vector<int, 4>)");
    
    // Insert an int vector
    session_.execute("INSERT INTO test_int (id, vec) VALUES (1, [1, 2, 3, 4])");
    
    // Query back
    Result result = session_.execute("SELECT vec FROM test_int WHERE id = 1");
    ASSERT_EQ(1ul, result.row_count());
    
    // Try to iterate
    Row row = result.first_row();
    const CassValue* vec_value = cass_row_get_column(row.get(), 0);
    CassIterator* iter = cass_iterator_from_vector(vec_value);
    if (iter) {
      cass_iterator_free(iter);
    }
    
    session_.execute("DROP TABLE test_int");
  }
  
  // Test 5: List inside vector (nested collection)
  {
    TEST_LOG("Testing vector<frozen<list<int>>, 2>...");
    session_.execute("CREATE TABLE IF NOT EXISTS test_nested ("
                     "id int PRIMARY KEY, "
                     "vec vector<frozen<list<int>>, 2>)");
    
    // Insert a vector of lists
    session_.execute("INSERT INTO test_nested (id, vec) VALUES (1, [[1, 2], [3, 4]])");
    
    // Query back
    Result result = session_.execute("SELECT vec FROM test_nested WHERE id = 1");
    ASSERT_EQ(1ul, result.row_count());
    
    // Try to iterate
    Row row = result.first_row();
    const CassValue* vec_value = cass_row_get_column(row.get(), 0);
    CassIterator* iter = cass_iterator_from_vector(vec_value);
    if (iter) {
      cass_iterator_free(iter);
    }
    
    session_.execute("DROP TABLE test_nested");
  }
  
  TEST_LOG("=== CHECK ERROR LOGS ABOVE FOR DEBUG OUTPUT ===");
  TEST_LOG("Look for [DEBUG] messages showing actual class names");
}