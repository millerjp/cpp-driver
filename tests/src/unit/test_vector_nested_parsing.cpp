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
#include "vector_type.hpp"
#include "data_type.hpp"

using namespace datastax::internal::core;

class VectorNestedParsingTest : public testing::Test {
public:
  void SetUp() {}
};

/**
 * Test parsing vector<frozen<list<int>>>
 */
TEST_F(VectorNestedParsingTest, ParseVectorOfFrozenListInt) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.FrozenType("
                      "org.apache.cassandra.db.marshal.ListType("
                      "org.apache.cassandra.db.marshal.Int32Type)), 3)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type) << "Failed to parse vector<frozen<list<int>>>";
  EXPECT_EQ(vector_type->dimension(), 3ul);
  
  // Element type should be a frozen list
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_LIST);
  
  const CollectionType* coll_type = static_cast<const CollectionType*>(vector_type->element_type().get());
  EXPECT_TRUE(coll_type->is_frozen());
  EXPECT_EQ(coll_type->types().size(), 1ul);
  
  // Inner element should be int
  ASSERT_TRUE(coll_type->types()[0]);
  EXPECT_EQ(coll_type->types()[0]->value_type(), CASS_VALUE_TYPE_INT);
}

/**
 * Test parsing vector<frozen<set<text>>>
 */
TEST_F(VectorNestedParsingTest, ParseVectorOfFrozenSetText) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.FrozenType("
                      "org.apache.cassandra.db.marshal.SetType("
                      "org.apache.cassandra.db.marshal.UTF8Type)), 5)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type) << "Failed to parse vector<frozen<set<text>>>";
  EXPECT_EQ(vector_type->dimension(), 5ul);
  
  // Element type should be a frozen set
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_SET);
  
  const CollectionType* coll_type = static_cast<const CollectionType*>(vector_type->element_type().get());
  EXPECT_TRUE(coll_type->is_frozen());
  
  // Inner element should be text
  ASSERT_TRUE(coll_type->types()[0]);
  EXPECT_EQ(coll_type->types()[0]->value_type(), CASS_VALUE_TYPE_TEXT);
}

/**
 * Test parsing vector<frozen<map<int, text>>>
 */
TEST_F(VectorNestedParsingTest, ParseVectorOfFrozenMapIntText) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.FrozenType("
                      "org.apache.cassandra.db.marshal.MapType("
                      "org.apache.cassandra.db.marshal.Int32Type,"
                      "org.apache.cassandra.db.marshal.UTF8Type)), 2)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type) << "Failed to parse vector<frozen<map<int, text>>>";
  EXPECT_EQ(vector_type->dimension(), 2ul);
  
  // Element type should be a frozen map
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_MAP);
  
  const CollectionType* coll_type = static_cast<const CollectionType*>(vector_type->element_type().get());
  EXPECT_TRUE(coll_type->is_frozen());
  EXPECT_EQ(coll_type->types().size(), 2ul);
  
  // Key should be int
  ASSERT_TRUE(coll_type->types()[0]);
  EXPECT_EQ(coll_type->types()[0]->value_type(), CASS_VALUE_TYPE_INT);
  
  // Value should be text
  ASSERT_TRUE(coll_type->types()[1]);
  EXPECT_EQ(coll_type->types()[1]->value_type(), CASS_VALUE_TYPE_TEXT);
}

/**
 * Test parsing vector<tuple<int, text>>
 */
TEST_F(VectorNestedParsingTest, ParseVectorOfTupleIntText) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.TupleType("
                      "org.apache.cassandra.db.marshal.Int32Type,"
                      "org.apache.cassandra.db.marshal.UTF8Type), 4)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type) << "Failed to parse vector<tuple<int, text>>";
  EXPECT_EQ(vector_type->dimension(), 4ul);
  
  // Element type should be a tuple
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_TUPLE);
  
  const TupleType* tuple_type = static_cast<const TupleType*>(vector_type->element_type().get());
  EXPECT_EQ(tuple_type->types().size(), 2ul);
  
  // First field should be int
  ASSERT_TRUE(tuple_type->types()[0]);
  EXPECT_EQ(tuple_type->types()[0]->value_type(), CASS_VALUE_TYPE_INT);
  
  // Second field should be text
  ASSERT_TRUE(tuple_type->types()[1]);
  EXPECT_EQ(tuple_type->types()[1]->value_type(), CASS_VALUE_TYPE_TEXT);
}

/**
 * Test parsing deeply nested: vector<frozen<list<frozen<set<uuid>>>>>
 */
TEST_F(VectorNestedParsingTest, ParseDeeplyNestedVector) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.FrozenType("
                      "org.apache.cassandra.db.marshal.ListType("
                      "org.apache.cassandra.db.marshal.FrozenType("
                      "org.apache.cassandra.db.marshal.SetType("
                      "org.apache.cassandra.db.marshal.UUIDType)))), 2)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type) << "Failed to parse deeply nested vector";
  EXPECT_EQ(vector_type->dimension(), 2ul);
  
  // Element should be frozen list
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_LIST);
  
  const CollectionType* list_type = static_cast<const CollectionType*>(vector_type->element_type().get());
  EXPECT_TRUE(list_type->is_frozen());
  
  // List element should be frozen set
  ASSERT_TRUE(list_type->types()[0]);
  EXPECT_EQ(list_type->types()[0]->value_type(), CASS_VALUE_TYPE_SET);
  
  const CollectionType* set_type = static_cast<const CollectionType*>(list_type->types()[0].get());
  EXPECT_TRUE(set_type->is_frozen());
  
  // Set element should be UUID
  ASSERT_TRUE(set_type->types()[0]);
  EXPECT_EQ(set_type->types()[0]->value_type(), CASS_VALUE_TYPE_UUID);
}

/**
 * Test parsing with spaces and formatting
 */
TEST_F(VectorNestedParsingTest, ParseWithSpaces) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType( "
                      "org.apache.cassandra.db.marshal.FrozenType( "
                      "org.apache.cassandra.db.marshal.ListType( "
                      "org.apache.cassandra.db.marshal.Int32Type ) ) , 3 )";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type) << "Failed to parse with spaces";
  EXPECT_EQ(vector_type->dimension(), 3ul);
}