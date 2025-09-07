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

#include <gtest/gtest.h>
#include <cassandra.h>

// Test that convenience APIs create vectors with correct element types
TEST(VectorConvenienceAPIsTest, CreateVectorWithList) {
  // Create vector<list<text>> using convenience API
  CassVector* vec = cass_vector_new_list(CASS_VALUE_TYPE_TEXT, 3);
  ASSERT_NE(vec, nullptr);
  
  // Verify the element data type is LIST
  const CassDataType* element_type = cass_vector_element_data_type(vec);
  ASSERT_NE(element_type, nullptr);
  EXPECT_EQ(cass_data_type_type(element_type), CASS_VALUE_TYPE_LIST);
  
  // Verify the list's element type is TEXT
  const CassDataType* list_element_type = cass_data_type_sub_data_type(element_type, 0);
  ASSERT_NE(list_element_type, nullptr);
  EXPECT_EQ(cass_data_type_type(list_element_type), CASS_VALUE_TYPE_TEXT);
  
  cass_vector_free(vec);
}

TEST(VectorConvenienceAPIsTest, CreateVectorWithSet) {
  // Create vector<set<int>> using convenience API
  CassVector* vec = cass_vector_new_set(CASS_VALUE_TYPE_INT, 2);
  ASSERT_NE(vec, nullptr);
  
  // Verify the element data type is SET
  const CassDataType* element_type = cass_vector_element_data_type(vec);
  ASSERT_NE(element_type, nullptr);
  EXPECT_EQ(cass_data_type_type(element_type), CASS_VALUE_TYPE_SET);
  
  // Verify the set's element type is INT
  const CassDataType* set_element_type = cass_data_type_sub_data_type(element_type, 0);
  ASSERT_NE(set_element_type, nullptr);
  EXPECT_EQ(cass_data_type_type(set_element_type), CASS_VALUE_TYPE_INT);
  
  cass_vector_free(vec);
}

TEST(VectorConvenienceAPIsTest, CreateVectorWithMap) {
  // Create vector<map<text, int>> using convenience API
  CassVector* vec = cass_vector_new_map(CASS_VALUE_TYPE_TEXT, CASS_VALUE_TYPE_INT, 4);
  ASSERT_NE(vec, nullptr);
  
  // Verify the element data type is MAP
  const CassDataType* element_type = cass_vector_element_data_type(vec);
  ASSERT_NE(element_type, nullptr);
  EXPECT_EQ(cass_data_type_type(element_type), CASS_VALUE_TYPE_MAP);
  
  // Verify the map's key type is TEXT
  const CassDataType* map_key_type = cass_data_type_sub_data_type(element_type, 0);
  ASSERT_NE(map_key_type, nullptr);
  EXPECT_EQ(cass_data_type_type(map_key_type), CASS_VALUE_TYPE_TEXT);
  
  // Verify the map's value type is INT
  const CassDataType* map_value_type = cass_data_type_sub_data_type(element_type, 1);
  ASSERT_NE(map_value_type, nullptr);
  EXPECT_EQ(cass_data_type_type(map_value_type), CASS_VALUE_TYPE_INT);
  
  cass_vector_free(vec);
}

TEST(VectorConvenienceAPIsTest, RejectComplexTypesInList) {
  // Should reject creating vector<list<list>>
  CassVector* vec = cass_vector_new_list(CASS_VALUE_TYPE_LIST, 2);
  EXPECT_EQ(vec, nullptr);
  
  // Should reject creating vector<list<map>>
  vec = cass_vector_new_list(CASS_VALUE_TYPE_MAP, 2);
  EXPECT_EQ(vec, nullptr);
}

TEST(VectorConvenienceAPIsTest, RejectComplexTypesInSet) {
  // Should reject creating vector<set<set>>
  CassVector* vec = cass_vector_new_set(CASS_VALUE_TYPE_SET, 2);
  EXPECT_EQ(vec, nullptr);
  
  // Should reject creating vector<set<tuple>>
  vec = cass_vector_new_set(CASS_VALUE_TYPE_TUPLE, 2);
  EXPECT_EQ(vec, nullptr);
}

TEST(VectorConvenienceAPIsTest, RejectComplexTypesInMap) {
  // Should reject creating vector<map<list, text>>
  CassVector* vec = cass_vector_new_map(CASS_VALUE_TYPE_LIST, CASS_VALUE_TYPE_TEXT, 2);
  EXPECT_EQ(vec, nullptr);
  
  // Should reject creating vector<map<text, udt>>
  vec = cass_vector_new_map(CASS_VALUE_TYPE_TEXT, CASS_VALUE_TYPE_UDT, 2);
  EXPECT_EQ(vec, nullptr);
}

TEST(VectorConvenienceAPIsTest, InvalidDimensions) {
  // Zero dimension
  CassVector* vec = cass_vector_new_list(CASS_VALUE_TYPE_TEXT, 0);
  EXPECT_EQ(vec, nullptr);
  
  // Too large dimension
  vec = cass_vector_new_set(CASS_VALUE_TYPE_INT, 8193);
  EXPECT_EQ(vec, nullptr);
}

TEST(VectorConvenienceAPIsTest, FunctionalUsage) {
  // Create vector<list<text>> and add a list to it
  CassVector* vec = cass_vector_new_list(CASS_VALUE_TYPE_TEXT, 2);
  ASSERT_NE(vec, nullptr);
  
  CassCollection* list = cass_collection_new(CASS_COLLECTION_TYPE_LIST, 2);
  cass_collection_append_string(list, "hello");
  cass_collection_append_string(list, "world");
  
  CassError error = cass_vector_append_collection(vec, list);
  EXPECT_EQ(error, CASS_OK);
  
  cass_collection_free(list);
  cass_vector_free(vec);
}