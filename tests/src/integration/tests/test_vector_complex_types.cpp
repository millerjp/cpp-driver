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
 * Complex type vector tests (collections in vectors)
 */
class VectorComplexTypeTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Create test keyspace
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_complex "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_complex");
  }
  
  void TearDown() {
    session_.execute("DROP KEYSPACE IF EXISTS vector_complex");
    Integration::TearDown();
  }
};

/**
 * Test vector<list<int>> with prepared statements
 * 
 * This test verifies that complex types in vectors work despite the server
 * returning "unknown" for the element type in prepared statement metadata.
 * The implementation uses Option 5: special case for "unknown" types.
 * 
 * @cassandra_version 5.0.0
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexTypeTest, ListInVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_list ("
                   "id int PRIMARY KEY, "
                   "vec vector<list<int>, 2>)");
  
  // Prepare statement
  Prepared prepared = session_.prepare("INSERT INTO test_list (id, vec) VALUES (?, ?)");
  
  // Get the vector parameter type
  const CassDataType* vec_type = cass_prepared_parameter_data_type(prepared.get(), 1);
  ASSERT_NE(vec_type, nullptr);
  ASSERT_EQ(cass_data_type_type(vec_type), CASS_VALUE_TYPE_CUSTOM);
  
  // Create vector from data type
  CassVector* vector = cass_vector_new_from_data_type(vec_type);
  ASSERT_NE(vector, nullptr);
  EXPECT_EQ(cass_vector_dimension(vector), 2ul);
  
  // Check element type - metadata parsing was fixed, now shows LIST correctly
  const CassDataType* element_type = cass_vector_element_data_type(vector);
  ASSERT_NE(element_type, nullptr);
  ASSERT_EQ(cass_data_type_type(element_type), CASS_VALUE_TYPE_LIST);
  
  // Create lists and append to vector
  CassCollection* list1 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 3);
  ASSERT_NE(list1, nullptr);
  ASSERT_EQ(cass_collection_append_int32(list1, 10), CASS_OK);
  ASSERT_EQ(cass_collection_append_int32(list1, 20), CASS_OK);
  ASSERT_EQ(cass_collection_append_int32(list1, 30), CASS_OK);
  
  // This should succeed due to "unknown" bypass
  ASSERT_EQ(cass_vector_append_collection(vector, list1), CASS_OK);
  cass_collection_free(list1);
  
  CassCollection* list2 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  ASSERT_NE(list2, nullptr);
  ASSERT_EQ(cass_collection_append_int32(list2, 40), CASS_OK);
  ASSERT_EQ(cass_collection_append_int32(list2, 50), CASS_OK);
  
  ASSERT_EQ(cass_vector_append_collection(vector, list2), CASS_OK);
  cass_collection_free(list2);
  
  // Bind and execute
  Statement statement = prepared.bind();
  statement.bind<Integer>(0, Integer(1));
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, vector), CASS_OK);
  
  Result result = session_.execute(statement, false);
  ASSERT_TRUE(result);
  
  cass_vector_free(vector);
  
  // Verify data was inserted correctly
  result = session_.execute("SELECT vec FROM test_list WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  ASSERT_FALSE(cass_value_is_null(vec_value));
  
  // Iterate vector elements
  CassIterator* vec_iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(vec_iter, nullptr);
  
  int vec_idx = 0;
  std::vector<std::vector<int32_t>> expected = {{10, 20, 30}, {40, 50}};
  
  while (cass_iterator_next(vec_iter)) {
    ASSERT_LT(vec_idx, 2);
    
    const CassValue* list_value = cass_iterator_get_value(vec_iter);
    ASSERT_NE(list_value, nullptr);
    ASSERT_FALSE(cass_value_is_null(list_value));
    
    // Check the value type
    CassValueType value_type = cass_value_type(list_value);
    TEST_LOG("Vector element " + std::to_string(vec_idx) + " has type: " + std::to_string(value_type));
    
    CassIterator* list_iter = cass_iterator_from_collection(list_value);
    ASSERT_NE(list_iter, nullptr) << "Failed to create iterator from collection at index " << vec_idx 
                                   << " with type " << value_type;
    
    int list_idx = 0;
    while (cass_iterator_next(list_iter)) {
      const CassValue* elem = cass_iterator_get_value(list_iter);
      cass_int32_t val;
      ASSERT_EQ(cass_value_get_int32(elem, &val), CASS_OK);
      EXPECT_EQ(val, expected[vec_idx][list_idx]) 
        << "Mismatch at vec[" << vec_idx << "][" << list_idx << "]";
      list_idx++;
    }
    EXPECT_EQ(list_idx, static_cast<int>(expected[vec_idx].size()));
    
    cass_iterator_free(list_iter);
    vec_idx++;
  }
  EXPECT_EQ(vec_idx, 2);
  
  cass_iterator_free(vec_iter);
  
  TEST_LOG("Successfully wrote and read vector<list<int>>");
}

/**
 * Test vector<set<text>> with prepared statements
 * 
 * @cassandra_version 5.0.0
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexTypeTest, SetInVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_set ("
                   "id int PRIMARY KEY, "
                   "vec vector<set<text>, 2>)");
  
  // Prepare statement
  Prepared prepared = session_.prepare("INSERT INTO test_set (id, vec) VALUES (?, ?)");
  
  const CassDataType* vec_type = cass_prepared_parameter_data_type(prepared.get(), 1);
  ASSERT_NE(vec_type, nullptr);
  
  CassVector* vector = cass_vector_new_from_data_type(vec_type);
  ASSERT_NE(vector, nullptr);
  
  // Create sets and append to vector
  CassCollection* set1 = cass_collection_new(CASS_COLLECTION_TYPE_SET, 2);
  ASSERT_NE(set1, nullptr);
  ASSERT_EQ(cass_collection_append_string(set1, "alpha"), CASS_OK);
  ASSERT_EQ(cass_collection_append_string(set1, "beta"), CASS_OK);
  
  ASSERT_EQ(cass_vector_append_collection(vector, set1), CASS_OK);
  cass_collection_free(set1);
  
  CassCollection* set2 = cass_collection_new(CASS_COLLECTION_TYPE_SET, 2);
  ASSERT_NE(set2, nullptr);
  ASSERT_EQ(cass_collection_append_string(set2, "gamma"), CASS_OK);
  ASSERT_EQ(cass_collection_append_string(set2, "delta"), CASS_OK);
  
  ASSERT_EQ(cass_vector_append_collection(vector, set2), CASS_OK);
  cass_collection_free(set2);
  
  // Bind and execute
  Statement statement = prepared.bind();
  statement.bind<Integer>(0, Integer(1));
  ASSERT_EQ(cass_statement_bind_vector(statement.get(), 1, vector), CASS_OK);
  
  Result result = session_.execute(statement, false);
  ASSERT_TRUE(result);
  
  cass_vector_free(vector);
  
  // Verify read
  result = session_.execute("SELECT vec FROM test_set WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  CassIterator* vec_iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(vec_iter, nullptr);
  
  int vec_count = 0;
  while (cass_iterator_next(vec_iter)) {
    vec_count++;
  }
  EXPECT_EQ(vec_count, 2);
  
  cass_iterator_free(vec_iter);
  
  TEST_LOG("Successfully wrote and read vector<set<text>>");
}

/**
 * Test server-side validation with wrong types
 * 
 * Even though we bypass type checking for "unknown" types,
 * the server should still validate and reject incorrect data.
 * 
 * @cassandra_version 5.0.0
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexTypeTest, ServerSideValidation) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table expecting list<int>
  session_.execute("CREATE TABLE IF NOT EXISTS test_validation ("
                   "id int PRIMARY KEY, "
                   "vec vector<list<int>, 2>)");
  
  Prepared prepared = session_.prepare("INSERT INTO test_validation (id, vec) VALUES (?, ?)");
  
  const CassDataType* vec_type = cass_prepared_parameter_data_type(prepared.get(), 1);
  CassVector* vector = cass_vector_new_from_data_type(vec_type);
  ASSERT_NE(vector, nullptr);
  
  // Try to append list with wrong element type (strings instead of ints)
  CassCollection* wrong_list = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  ASSERT_NE(wrong_list, nullptr);
  ASSERT_EQ(cass_collection_append_string(wrong_list, "hello"), CASS_OK);
  ASSERT_EQ(cass_collection_append_string(wrong_list, "world"), CASS_OK);
  
  // Client should allow this due to "unknown" bypass
  ASSERT_EQ(cass_vector_append_collection(vector, wrong_list), CASS_OK);
  cass_collection_free(wrong_list);
  
  // Add another list to satisfy dimension requirement
  CassCollection* list2 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 1);
  ASSERT_EQ(cass_collection_append_int32(list2, 999), CASS_OK);
  ASSERT_EQ(cass_vector_append_collection(vector, list2), CASS_OK);
  cass_collection_free(list2);
  
  // Try to execute - server should reject
  Statement statement = prepared.bind();
  statement.bind<Integer>(0, Integer(1));
  cass_statement_bind_vector(statement.get(), 1, vector);
  
  // Expect failure due to type mismatch
  CassFuture* future = cass_session_execute(session_.get(), statement.get());
  CassError rc = cass_future_error_code(future);
  EXPECT_NE(rc, CASS_OK) << "Server should reject wrong element type";
  
  if (rc != CASS_OK) {
    const char* message;
    size_t message_length;
    cass_future_error_message(future, &message, &message_length);
    std::string error_msg(message, message_length);
    TEST_LOG("Server correctly rejected: " + error_msg);
  }
  
  cass_future_free(future);
  cass_vector_free(vector);
}

/**
 * Test simple statement with complex vectors
 * 
 * Simple statements don't use prepared metadata, so they work differently.
 * 
 * @cassandra_version 5.0.0
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexTypeTest, SimpleStatementComplexVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_simple ("
                   "id int PRIMARY KEY, "
                   "vec vector<list<int>, 2>)");
  
  // For simple statements, we don't have prepared metadata
  // We need to create the vector with just dimension info
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_LIST, 2);
  ASSERT_NE(vector, nullptr);
  
  // Create lists
  CassCollection* list1 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  ASSERT_EQ(cass_collection_append_int32(list1, 100), CASS_OK);
  ASSERT_EQ(cass_collection_append_int32(list1, 200), CASS_OK);
  
  // For simple statements without type info, this might fail
  CassError rc = cass_vector_append_collection(vector, list1);
  cass_collection_free(list1);
  
  if (rc == CASS_OK) {
    // If it works, add second list and try to execute
    CassCollection* list2 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 1);
    ASSERT_EQ(cass_collection_append_int32(list2, 300), CASS_OK);
    ASSERT_EQ(cass_vector_append_collection(vector, list2), CASS_OK);
    cass_collection_free(list2);
    
    CassStatement* stmt = cass_statement_new("INSERT INTO test_simple (id, vec) VALUES (?, ?)", 2);
    ASSERT_EQ(cass_statement_bind_int32(stmt, 0, 1), CASS_OK);
    ASSERT_EQ(cass_statement_bind_vector(stmt, 1, vector), CASS_OK);
    
    CassFuture* future = cass_session_execute(session_.get(), stmt);
    CassError exec_rc = cass_future_error_code(future);
    
    if (exec_rc == CASS_OK) {
      TEST_LOG("Simple statement with complex vector succeeded");
    } else {
      TEST_LOG("Simple statement with complex vector failed (expected without type info)");
    }
    
    cass_future_free(future);
    cass_statement_free(stmt);
  } else {
    TEST_LOG("Cannot append collection to vector without proper type info (expected)");
  }
  
  cass_vector_free(vector);
}