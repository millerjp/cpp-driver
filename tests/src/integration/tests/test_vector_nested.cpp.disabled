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
#include <vector>

/**
 * Nested collection vector integration tests
 */
class VectorNestedTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Create test keyspace
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_nested "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_nested");
  }
  
  void TearDown() {
    session_.execute("DROP KEYSPACE IF EXISTS vector_nested");
    Integration::TearDown();
  }
};

/**
 * Test vector<frozen<list<int>>> round-trip
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNestedTest, VectorOfListInt) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table with vector of frozen lists
  session_.execute("CREATE TABLE IF NOT EXISTS vector_list ("
                   "id int PRIMARY KEY, "
                   "vec vector<frozen<list<int>>, 2>)");
  
  // Create vector with lists as elements
  CassVector* vector = cass_vector_new_with_element_type(
      DataType::ConstPtr(new CollectionType(CASS_COLLECTION_TYPE_LIST, 
                                           DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_INT)), 
                                           true)).get(), 2);
  ASSERT_NE(vector, nullptr);
  
  // Create first list: [1, 2, 3]
  CassCollection* list1 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 3);
  cass_collection_append_int32(list1, 1);
  cass_collection_append_int32(list1, 2);
  cass_collection_append_int32(list1, 3);
  
  // Create second list: [4, 5]
  CassCollection* list2 = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  cass_collection_append_int32(list2, 4);
  cass_collection_append_int32(list2, 5);
  
  // Add lists to vector
  ASSERT_EQ(cass_vector_append_collection(vector, list1), CASS_OK);
  ASSERT_EQ(cass_vector_append_collection(vector, list2), CASS_OK);
  
  cass_collection_free(list1);
  cass_collection_free(list2);
  
  // Insert using prepared statement
  Prepared prepared = session_.prepare("INSERT INTO vector_list (id, vec) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  session_.execute(stmt);
  cass_vector_free(vector);
  
  // Query back and verify
  Result result = session_.execute("SELECT vec FROM vector_list WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  // Create iterator for vector
  CassIterator* vec_iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(vec_iter, nullptr) << "Failed to create iterator for vector<frozen<list<int>>>";
  
  // First element should be list [1, 2, 3]
  ASSERT_TRUE(cass_iterator_next(vec_iter));
  const CassValue* list1_value = cass_iterator_get_value(vec_iter);
  CassIterator* list1_iter = cass_iterator_from_collection(list1_value);
  ASSERT_NE(list1_iter, nullptr);
  
  std::vector<int32_t> list1_values;
  while (cass_iterator_next(list1_iter)) {
    const CassValue* element = cass_iterator_get_value(list1_iter);
    int32_t val;
    ASSERT_EQ(cass_value_get_int32(element, &val), CASS_OK);
    list1_values.push_back(val);
  }
  cass_iterator_free(list1_iter);
  
  ASSERT_EQ(list1_values.size(), 3ul);
  EXPECT_EQ(list1_values[0], 1);
  EXPECT_EQ(list1_values[1], 2);
  EXPECT_EQ(list1_values[2], 3);
  
  // Second element should be list [4, 5]
  ASSERT_TRUE(cass_iterator_next(vec_iter));
  const CassValue* list2_value = cass_iterator_get_value(vec_iter);
  CassIterator* list2_iter = cass_iterator_from_collection(list2_value);
  ASSERT_NE(list2_iter, nullptr);
  
  std::vector<int32_t> list2_values;
  while (cass_iterator_next(list2_iter)) {
    const CassValue* element = cass_iterator_get_value(list2_iter);
    int32_t val;
    ASSERT_EQ(cass_value_get_int32(element, &val), CASS_OK);
    list2_values.push_back(val);
  }
  cass_iterator_free(list2_iter);
  
  ASSERT_EQ(list2_values.size(), 2ul);
  EXPECT_EQ(list2_values[0], 4);
  EXPECT_EQ(list2_values[1], 5);
  
  // No more elements
  EXPECT_FALSE(cass_iterator_next(vec_iter));
  cass_iterator_free(vec_iter);
  
  TEST_LOG("vector<frozen<list<int>>> round-trip successful!");
}

/**
 * Test vector<frozen<set<text>>> round-trip
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNestedTest, VectorOfSetText) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table with vector of frozen sets
  session_.execute("CREATE TABLE IF NOT EXISTS vector_set ("
                   "id int PRIMARY KEY, "
                   "vec vector<frozen<set<text>>, 2>)");
  
  // Create vector with sets as elements
  CassVector* vector = cass_vector_new_with_element_type(
      DataType::ConstPtr(new CollectionType(CASS_COLLECTION_TYPE_SET, 
                                           DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TEXT)), 
                                           true)).get(), 2);
  ASSERT_NE(vector, nullptr);
  
  // Create first set: {"apple", "banana"}
  CassCollection* set1 = cass_collection_new(CASS_COLLECTION_TYPE_SET, 2);
  cass_collection_append_string(set1, "apple");
  cass_collection_append_string(set1, "banana");
  
  // Create second set: {"cherry", "date", "elderberry"}
  CassCollection* set2 = cass_collection_new(CASS_COLLECTION_TYPE_SET, 3);
  cass_collection_append_string(set2, "cherry");
  cass_collection_append_string(set2, "date");
  cass_collection_append_string(set2, "elderberry");
  
  // Add sets to vector
  ASSERT_EQ(cass_vector_append_collection(vector, set1), CASS_OK);
  ASSERT_EQ(cass_vector_append_collection(vector, set2), CASS_OK);
  
  cass_collection_free(set1);
  cass_collection_free(set2);
  
  // Insert
  Prepared prepared = session_.prepare("INSERT INTO vector_set (id, vec) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  session_.execute(stmt);
  cass_vector_free(vector);
  
  // Query back
  Result result = session_.execute("SELECT vec FROM vector_set WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  // Create iterator for vector
  CassIterator* vec_iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(vec_iter, nullptr) << "Failed to create iterator for vector<frozen<set<text>>>";
  
  // Iterate through sets and collect all values
  int set_count = 0;
  while (cass_iterator_next(vec_iter)) {
    const CassValue* set_value = cass_iterator_get_value(vec_iter);
    CassIterator* set_iter = cass_iterator_from_collection(set_value);
    ASSERT_NE(set_iter, nullptr);
    
    while (cass_iterator_next(set_iter)) {
      const CassValue* element = cass_iterator_get_value(set_iter);
      const char* str;
      size_t len;
      ASSERT_EQ(cass_value_get_string(element, &str, &len), CASS_OK);
      TEST_LOG("Set %d contains: %.*s", set_count, (int)len, str);
    }
    cass_iterator_free(set_iter);
    set_count++;
  }
  
  EXPECT_EQ(set_count, 2);
  cass_iterator_free(vec_iter);
  
  TEST_LOG("vector<frozen<set<text>>> round-trip successful!");
}

/**
 * Test vector<frozen<map<int, text>>> round-trip
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNestedTest, VectorOfMapIntText) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table with vector of frozen maps
  session_.execute("CREATE TABLE IF NOT EXISTS vector_map ("
                   "id int PRIMARY KEY, "
                   "vec vector<frozen<map<int, text>>, 2>)");
  
  // Create vector with maps as elements
  CassVector* vector = cass_vector_new_with_element_type(
      DataType::ConstPtr(new CollectionType(CASS_COLLECTION_TYPE_MAP, 
                                           DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_INT)),
                                           DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TEXT)),
                                           true)).get(), 2);
  ASSERT_NE(vector, nullptr);
  
  // Create first map: {1: "one", 2: "two"}
  CassCollection* map1 = cass_collection_new(CASS_COLLECTION_TYPE_MAP, 2);
  cass_collection_append_int32(map1, 1);
  cass_collection_append_string(map1, "one");
  cass_collection_append_int32(map1, 2);
  cass_collection_append_string(map1, "two");
  
  // Create second map: {10: "ten", 20: "twenty", 30: "thirty"}
  CassCollection* map2 = cass_collection_new(CASS_COLLECTION_TYPE_MAP, 3);
  cass_collection_append_int32(map2, 10);
  cass_collection_append_string(map2, "ten");
  cass_collection_append_int32(map2, 20);
  cass_collection_append_string(map2, "twenty");
  cass_collection_append_int32(map2, 30);
  cass_collection_append_string(map2, "thirty");
  
  // Add maps to vector
  ASSERT_EQ(cass_vector_append_collection(vector, map1), CASS_OK);
  ASSERT_EQ(cass_vector_append_collection(vector, map2), CASS_OK);
  
  cass_collection_free(map1);
  cass_collection_free(map2);
  
  // Insert
  Prepared prepared = session_.prepare("INSERT INTO vector_map (id, vec) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  session_.execute(stmt);
  cass_vector_free(vector);
  
  // Query back
  Result result = session_.execute("SELECT vec FROM vector_map WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  // Create iterator for vector
  CassIterator* vec_iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(vec_iter, nullptr) << "Failed to create iterator for vector<frozen<map<int, text>>>";
  
  // Verify we can iterate through maps
  int map_count = 0;
  while (cass_iterator_next(vec_iter)) {
    const CassValue* map_value = cass_iterator_get_value(vec_iter);
    CassIterator* map_iter = cass_iterator_from_map(map_value);
    ASSERT_NE(map_iter, nullptr);
    
    int entry_count = 0;
    while (cass_iterator_next(map_iter)) {
      const CassValue* key = cass_iterator_get_map_key(map_iter);
      const CassValue* value = cass_iterator_get_map_value(map_iter);
      
      int32_t key_val;
      ASSERT_EQ(cass_value_get_int32(key, &key_val), CASS_OK);
      
      const char* str;
      size_t len;
      ASSERT_EQ(cass_value_get_string(value, &str, &len), CASS_OK);
      
      TEST_LOG("Map %d entry: %d -> %.*s", map_count, key_val, (int)len, str);
      entry_count++;
    }
    cass_iterator_free(map_iter);
    
    EXPECT_GT(entry_count, 0);
    map_count++;
  }
  
  EXPECT_EQ(map_count, 2);
  cass_iterator_free(vec_iter);
  
  TEST_LOG("vector<frozen<map<int, text>>> round-trip successful!");
}

/**
 * Test vector<tuple<int, text>> round-trip
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNestedTest, VectorOfTuple) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table with vector of tuples
  session_.execute("CREATE TABLE IF NOT EXISTS vector_tuple ("
                   "id int PRIMARY KEY, "
                   "vec vector<tuple<int, text>, 3>)");
  
  // Create tuple type
  DataType::Vec tuple_types;
  tuple_types.push_back(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_INT)));
  tuple_types.push_back(DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TEXT)));
  DataType::ConstPtr tuple_type(new TupleType(tuple_types, false));
  
  // Create vector with tuples as elements
  CassVector* vector = cass_vector_new_with_element_type(tuple_type.get(), 3);
  ASSERT_NE(vector, nullptr);
  
  // Create and add tuples
  CassTuple* tuple1 = cass_tuple_new(2);
  cass_tuple_set_int32(tuple1, 0, 1);
  cass_tuple_set_string(tuple1, 1, "first");
  
  CassTuple* tuple2 = cass_tuple_new(2);
  cass_tuple_set_int32(tuple2, 0, 2);
  cass_tuple_set_string(tuple2, 1, "second");
  
  CassTuple* tuple3 = cass_tuple_new(2);
  cass_tuple_set_int32(tuple3, 0, 3);
  cass_tuple_set_string(tuple3, 1, "third");
  
  ASSERT_EQ(cass_vector_append_tuple(vector, tuple1), CASS_OK);
  ASSERT_EQ(cass_vector_append_tuple(vector, tuple2), CASS_OK);
  ASSERT_EQ(cass_vector_append_tuple(vector, tuple3), CASS_OK);
  
  cass_tuple_free(tuple1);
  cass_tuple_free(tuple2);
  cass_tuple_free(tuple3);
  
  // Insert
  Prepared prepared = session_.prepare("INSERT INTO vector_tuple (id, vec) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  session_.execute(stmt);
  cass_vector_free(vector);
  
  // Query back
  Result result = session_.execute("SELECT vec FROM vector_tuple WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  // Create iterator for vector
  CassIterator* vec_iter = cass_iterator_from_vector(vec_value);
  ASSERT_NE(vec_iter, nullptr) << "Failed to create iterator for vector<tuple<int, text>>";
  
  // Verify tuples
  int tuple_count = 0;
  while (cass_iterator_next(vec_iter)) {
    const CassValue* tuple_value = cass_iterator_get_value(vec_iter);
    CassIterator* tuple_iter = cass_iterator_from_tuple(tuple_value);
    ASSERT_NE(tuple_iter, nullptr);
    
    // First field: int
    ASSERT_TRUE(cass_iterator_next(tuple_iter));
    const CassValue* int_field = cass_iterator_get_value(tuple_iter);
    int32_t int_val;
    ASSERT_EQ(cass_value_get_int32(int_field, &int_val), CASS_OK);
    
    // Second field: text
    ASSERT_TRUE(cass_iterator_next(tuple_iter));
    const CassValue* text_field = cass_iterator_get_value(tuple_iter);
    const char* str;
    size_t len;
    ASSERT_EQ(cass_value_get_string(text_field, &str, &len), CASS_OK);
    
    TEST_LOG("Tuple %d: (%d, %.*s)", tuple_count, int_val, (int)len, str);
    
    cass_iterator_free(tuple_iter);
    tuple_count++;
  }
  
  EXPECT_EQ(tuple_count, 3);
  cass_iterator_free(vec_iter);
  
  TEST_LOG("vector<tuple<int, text>> round-trip successful!");
}