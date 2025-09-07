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
 * Nested vector tests (vector of vectors and vector of collections)
 */
class VectorNestedSimpleTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
  }
};

/**
 * Test vector of frozen lists
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Vector of lists works correctly
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNestedSimpleTest, VectorOfLists) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create keyspace
  session_.execute(
      format_string("CREATE KEYSPACE IF NOT EXISTS %s "
                   "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}", 
                   keyspace_name_.c_str()));
  session_.execute("USE " + keyspace_name_);
  
  // Create table with vector of frozen lists
  session_.execute("CREATE TABLE IF NOT EXISTS vector_lists ("
                   "id int PRIMARY KEY, "
                   "lists vector<frozen<list<int>>, 2>)");
  
  // For simple statements with complex vectors, we need to use the new API
  // that allows specifying the full element type
  
  // Create the element type: list<int>
  CassDataType* int_type = cass_data_type_new(CASS_VALUE_TYPE_INT);
  CassDataType* list_type = cass_data_type_new(CASS_VALUE_TYPE_LIST);
  cass_data_type_add_sub_type(list_type, int_type);
  cass_data_type_free(int_type);
  
  // Create vector with the proper element type
  CassVector* vec = cass_vector_new_with_element_type(list_type, 2);
  ASSERT_NE(vec, nullptr) << "Failed to create vector with list<int> element type";
  
  // Insert using simple statement
  const char* query = "INSERT INTO vector_lists (id, lists) VALUES (?, ?)";
  CassStatement* stmt = cass_statement_new(query, 2);
  cass_statement_bind_int32(stmt, 0, 1);
  
  // Create first list [1, 2, 3]
  CassCollection* list1 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 3);
  cass_collection_append_int32(list1, 1);
  cass_collection_append_int32(list1, 2);
  cass_collection_append_int32(list1, 3);
  cass_vector_append_collection(vec, list1);
  cass_collection_free(list1);
  
  // Create second list [4, 5]
  CassCollection* list2 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  cass_collection_append_int32(list2, 4);
  cass_collection_append_int32(list2, 5);
  cass_vector_append_collection(vec, list2);
  cass_collection_free(list2);
  
  cass_statement_bind_vector(stmt, 1, vec);
  cass_vector_free(vec);
  cass_data_type_free(list_type);
  
  CassFuture* future = cass_session_execute(session_.get(), stmt);
  cass_statement_free(stmt);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify data was inserted
  Result result = session_.execute("SELECT * FROM vector_lists WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test vector of frozen sets
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Vector of sets works correctly
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNestedSimpleTest, VectorOfSets) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create keyspace
  session_.execute(
      format_string("CREATE KEYSPACE IF NOT EXISTS %s "
                   "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}", 
                   keyspace_name_.c_str()));
  session_.execute("USE " + keyspace_name_);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS vector_sets ("
                   "id int PRIMARY KEY, "
                   "sets vector<frozen<set<text>>, 3>)");
  
  // Create vector<set<text>> using the new convenience API
  // Much simpler than manually constructing the data type!
  CassVector* vec = cass_vector_new_set(CASS_VALUE_TYPE_TEXT, 3);
  ASSERT_NE(vec, nullptr) << "Failed to create vector with set<text> element type";
  
  // Insert using simple statement
  const char* query = "INSERT INTO vector_sets (id, sets) VALUES (?, ?)";
  CassStatement* stmt = cass_statement_new(query, 2);
  cass_statement_bind_int32(stmt, 0, 1);
  
  // Create three sets
  for (int i = 0; i < 3; ++i) {
    CassCollection* set = cass_collection_new(CASS_COLLECTION_TYPE_SET, 2);
    char str1[10], str2[10];
    snprintf(str1, sizeof(str1), "a%d", i);
    snprintf(str2, sizeof(str2), "b%d", i);
    cass_collection_append_string(set, str1);
    cass_collection_append_string(set, str2);
    cass_vector_append_collection(vec, set);
    cass_collection_free(set);
  }
  
  cass_statement_bind_vector(stmt, 1, vec);
  cass_vector_free(vec);
  cass_data_type_free(set_type);
  
  CassFuture* future = cass_session_execute(session_.get(), stmt);
  cass_statement_free(stmt);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT COUNT(*) FROM vector_sets");
  ASSERT_EQ(1, result.first_row().column_by_name<BigInteger>("count").value());
}

/**
 * Test vector of frozen maps
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Vector of maps works correctly
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNestedSimpleTest, VectorOfMaps) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create keyspace
  session_.execute(
      format_string("CREATE KEYSPACE IF NOT EXISTS %s "
                   "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}", 
                   keyspace_name_.c_str()));
  session_.execute("USE " + keyspace_name_);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS vector_maps ("
                   "id int PRIMARY KEY, "
                   "maps vector<frozen<map<text, int>>, 2>)");
  
  // Insert using CQL (simpler approach)
  session_.execute("INSERT INTO vector_maps (id, maps) VALUES (1, "
                   "[{'key1': 10, 'key2': 20}, {'key3': 30}])");
  
  // Verify
  Result result = session_.execute("SELECT * FROM vector_maps WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test vector of frozen vectors (nested vectors)
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Nested vectors can be created in schema
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNestedSimpleTest, VectorOfVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create keyspace
  session_.execute(
      format_string("CREATE KEYSPACE IF NOT EXISTS %s "
                   "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}", 
                   keyspace_name_.c_str()));
  session_.execute("USE " + keyspace_name_);
  
  // Try to create table with nested vectors
  session_.execute("CREATE TABLE IF NOT EXISTS nested_vectors ("
                   "id int PRIMARY KEY, "
                   "vecs vector<frozen<vector<int, 2>>, 3>)");
  
  // Insert using CQL
  session_.execute("INSERT INTO nested_vectors (id, vecs) VALUES (1, "
                   "[[1, 2], [3, 4], [5, 6]])");
  
  // Verify table exists and data was inserted
  Result result = session_.execute("SELECT * FROM nested_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  // For vector of vectors, we can't easily create the type programmatically
  // because vectors are CUSTOM types, not regular types.
  // This would require creating a full VectorType object which isn't exposed in the C API.
  // For now, we'll skip the C API test for nested vectors and just verify the CQL works.
  
  // The CQL insert above already verified this works
  TEST_LOG("Vector of vectors works with CQL literals");
  TEST_LOG("C API for nested vectors requires prepared statements for type metadata");
}