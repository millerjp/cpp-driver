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

#include "cassandra.h"
#include "test_utils.hpp"
#include "cass_vector.hpp"
#include "vector_type.hpp"

using namespace datastax::internal::core;

// Test that unknown element types are properly rejected
TEST(VectorUnknownTest, RejectUnknownElementType) {
  // Create a VectorType with "unknown" element type to simulate the old bug
  DataType::ConstPtr unknown_element(new CustomType("unknown"));
  VectorType::ConstPtr vector_type(new VectorType(unknown_element, 3));
  
  // Create a vector with this type
  CassandraVector vector(vector_type);
  
  // Try to append a value - should fail with CASS_ERROR_LIB_INVALID_CUSTOM_TYPE
  CassError error = vector.append(123);
  EXPECT_EQ(error, CASS_ERROR_LIB_INVALID_CUSTOM_TYPE) 
    << "Unknown element types should be rejected, not silently accepted";
  
  // Try with a collection
  CollectionType::ConstPtr list_type = CollectionType::list(
    DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_INT)), false);
  Collection collection(list_type, 1);
  collection.append(42);
  
  error = vector.append(&collection);
  EXPECT_EQ(error, CASS_ERROR_LIB_INVALID_CUSTOM_TYPE)
    << "Collections with unknown vector element type should be rejected";
}

// Test that known types still work
TEST(VectorUnknownTest, AcceptKnownElementTypes) {
  // Create a proper VectorType with INT element
  DataType::ConstPtr int_element(new DataType(CASS_VALUE_TYPE_INT));
  VectorType::ConstPtr vector_type(new VectorType(int_element, 3));
  
  CassandraVector vector(vector_type);
  
  // This should work fine
  EXPECT_EQ(vector.append(123), CASS_OK);
  EXPECT_EQ(vector.append(456), CASS_OK);
  EXPECT_EQ(vector.append(789), CASS_OK);
  
  // Fourth element should fail (dimension exceeded)
  EXPECT_EQ(vector.append(999), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
}

// Test complex vectors with proper metadata
TEST(VectorUnknownTest, ComplexVectorsWithMetadata) {
  // Create vector<list<int>, 2> with proper metadata
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  CollectionType::ConstPtr list_type = CollectionType::list(int_type, false);
  VectorType::ConstPtr vector_type(new VectorType(list_type, 2));
  
  // Preserve the class name as would happen after metadata fix
  VectorType* mutable_vector = const_cast<VectorType*>(vector_type.get());
  mutable_vector->set_class_name(
    "org.apache.cassandra.db.marshal.VectorType("
    "org.apache.cassandra.db.marshal.ListType("
    "org.apache.cassandra.db.marshal.Int32Type), 2)");
  
  CassandraVector vector(vector_type);
  
  // Create a list with proper type info
  Collection list1(list_type, 2);
  list1.append(10);
  list1.append(20);
  
  // This should work because types match
  EXPECT_EQ(vector.append(&list1), CASS_OK);
  
  // Try wrong collection type (SET instead of LIST)
  CollectionType::ConstPtr set_type = CollectionType::set(int_type, false);
  Collection wrong_set(set_type, 2);
  wrong_set.append(30);
  wrong_set.append(40);
  
  // This should fail due to type mismatch
  EXPECT_NE(vector.append(&wrong_set), CASS_OK)
    << "Wrong collection type should be rejected";
}