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
#include "string.hpp"

using namespace datastax;
using namespace datastax::internal::core;

class VectorTypeParsingTest : public testing::Test {
public:
  void SetUp() {}
};

/**
 * Test parsing float vector type
 */
TEST_F(VectorTypeParsingTest, ParseFloatVector) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.FloatType, 3)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type);
  EXPECT_EQ(vector_type->dimension(), 3ul);
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_FLOAT);
  EXPECT_TRUE(vector_type->is_fixed_length_element());
}

/**
 * Test parsing int vector type
 */
TEST_F(VectorTypeParsingTest, ParseIntVector) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.Int32Type, 5)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type);
  EXPECT_EQ(vector_type->dimension(), 5ul);
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_INT);
  EXPECT_TRUE(vector_type->is_fixed_length_element());
}

/**
 * Test parsing text vector type
 */
TEST_F(VectorTypeParsingTest, ParseTextVector) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.UTF8Type, 2)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type);
  EXPECT_EQ(vector_type->dimension(), 2ul);
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_TEXT);
  EXPECT_FALSE(vector_type->is_fixed_length_element());  // Text is variable length
}

/**
 * Test parsing blob vector type
 */
TEST_F(VectorTypeParsingTest, ParseBlobVector) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.BytesType, 4)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type);
  EXPECT_EQ(vector_type->dimension(), 4ul);
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_BLOB);
  EXPECT_FALSE(vector_type->is_fixed_length_element());  // Blob is variable length
}

/**
 * Test parsing UUID vector type
 */
TEST_F(VectorTypeParsingTest, ParseUUIDVector) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.UUIDType, 2)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type);
  EXPECT_EQ(vector_type->dimension(), 2ul);
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_UUID);
  EXPECT_TRUE(vector_type->is_fixed_length_element());  // UUID is fixed 16 bytes
}

/**
 * Test parsing fails on invalid class name
 */
TEST_F(VectorTypeParsingTest, FailOnInvalidClassName) {
  String class_name = "not.a.vector.type";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  EXPECT_FALSE(vector_type);
}

/**
 * Test parsing fails on malformed vector
 */
TEST_F(VectorTypeParsingTest, FailOnMalformedVector) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType(malformed)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  EXPECT_FALSE(vector_type);
}

/**
 * Test parsing succeeds on unknown element type (Option 5 implementation)
 */
TEST_F(VectorTypeParsingTest, SucceedOnUnknownElementType) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.UnknownType, 3)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type);  // Should succeed with unknown type support
  EXPECT_EQ(vector_type->dimension(), 3u);
  // Unknown types are represented as CUSTOM types internally
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_CUSTOM);
}

/**
 * Test parsing succeeds on nested collection (now supported)
 */
TEST_F(VectorTypeParsingTest, SucceedOnNestedCollection) {
  String class_name = "org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.ListType(org.apache.cassandra.db.marshal.Int32Type), 2)";
  
  VectorType::ConstPtr vector_type = VectorType::from_class_name(class_name);
  ASSERT_TRUE(vector_type);  // Should succeed with nested collection support
  EXPECT_EQ(vector_type->dimension(), 2u);
  ASSERT_TRUE(vector_type->element_type());
  EXPECT_EQ(vector_type->element_type()->value_type(), CASS_VALUE_TYPE_LIST);
}