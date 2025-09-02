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
#include "cass_vector.hpp"
#include "vector_type.hpp"
#include "value.hpp"
#include "serialization.hpp"
#include "collection_iterator.hpp"
#include <cstring>

using namespace datastax::internal::core;

class VectorIteratorTest : public ::testing::Test {
protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(VectorIteratorTest, IterateFloatVector) {
  // Create a vector with float elements
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  CassandraVector vector(float_type, 3);
  
  // Add three floats
  ASSERT_EQ(vector.append(1.0f), CASS_OK);
  ASSERT_EQ(vector.append(2.0f), CASS_OK);
  ASSERT_EQ(vector.append(3.0f), CASS_OK);
  
  // Encode the vector
  Buffer encoded = vector.encode_with_length();
  
  // Create a Value from the encoded data
  VectorType::ConstPtr vector_type(new VectorType(float_type, 3));
  Decoder decoder(encoded.data() + sizeof(int32_t), encoded.size() - sizeof(int32_t), 4);
  Value value(vector_type, decoder);
  
  // Create an iterator
  VectorIterator iterator(&value);
  
  // Iterate and verify values
  float expected[] = {1.0f, 2.0f, 3.0f};
  int index = 0;
  
  while (iterator.next()) {
    const Value* element = iterator.value();
    ASSERT_FALSE(element->is_null());
    ASSERT_EQ(element->value_type(), CASS_VALUE_TYPE_FLOAT);
    
    // Get the float value
    cass_float_t actual;
    ASSERT_EQ(cass_value_get_float(element, &actual), CASS_OK);
    EXPECT_FLOAT_EQ(actual, expected[index]);
    index++;
  }
  
  EXPECT_EQ(index, 3);
}

TEST_F(VectorIteratorTest, IterateIntVector) {
  // Create a vector with int elements
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  CassandraVector vector(int_type, 4);
  
  // Add four ints
  ASSERT_EQ(vector.append(10), CASS_OK);
  ASSERT_EQ(vector.append(20), CASS_OK);
  ASSERT_EQ(vector.append(30), CASS_OK);
  ASSERT_EQ(vector.append(40), CASS_OK);
  
  // Encode the vector
  Buffer encoded = vector.encode_with_length();
  
  // Create a Value from the encoded data
  VectorType::ConstPtr vector_type(new VectorType(int_type, 4));
  Decoder decoder(encoded.data() + sizeof(int32_t), encoded.size() - sizeof(int32_t), 4);
  Value value(vector_type, decoder);
  
  // Create an iterator
  VectorIterator iterator(&value);
  
  // Iterate and verify values
  int32_t expected[] = {10, 20, 30, 40};
  int index = 0;
  
  while (iterator.next()) {
    const Value* element = iterator.value();
    ASSERT_FALSE(element->is_null());
    ASSERT_EQ(element->value_type(), CASS_VALUE_TYPE_INT);
    
    // Get the int value
    cass_int32_t actual;
    ASSERT_EQ(cass_value_get_int32(element, &actual), CASS_OK);
    EXPECT_EQ(actual, expected[index]);
    index++;
  }
  
  EXPECT_EQ(index, 4);
}

TEST_F(VectorIteratorTest, IterateTextVector) {
  // Create a vector with text elements (variable-length)
  DataType::ConstPtr text_type(new DataType(CASS_VALUE_TYPE_TEXT));
  CassandraVector vector(text_type, 3);
  
  // Add three strings
  ASSERT_EQ(vector.append(CassString("hello", 5)), CASS_OK);
  ASSERT_EQ(vector.append(CassString("world", 5)), CASS_OK);
  ASSERT_EQ(vector.append(CassString("test", 4)), CASS_OK);
  
  // Encode the vector
  Buffer encoded = vector.encode_with_length();
  
  // Create a Value from the encoded data
  VectorType::ConstPtr vector_type(new VectorType(text_type, 3));
  Decoder decoder(encoded.data() + sizeof(int32_t), encoded.size() - sizeof(int32_t), 4);
  Value value(vector_type, decoder);
  
  // Create an iterator
  VectorIterator iterator(&value);
  
  // Iterate and verify values
  const char* expected[] = {"hello", "world", "test"};
  size_t expected_lengths[] = {5, 5, 4};
  int index = 0;
  
  while (iterator.next()) {
    const Value* element = iterator.value();
    ASSERT_FALSE(element->is_null());
    ASSERT_EQ(element->value_type(), CASS_VALUE_TYPE_TEXT);
    
    // Get the string value
    const char* actual;
    size_t actual_length;
    ASSERT_EQ(cass_value_get_string(element, &actual, &actual_length), CASS_OK);
    EXPECT_EQ(actual_length, expected_lengths[index]);
    EXPECT_EQ(memcmp(actual, expected[index], actual_length), 0);
    index++;
  }
  
  EXPECT_EQ(index, 3);
}

TEST_F(VectorIteratorTest, EmptyVector) {
  // Create an empty vector (dimension 0)
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  CassandraVector vector(float_type, 0);
  
  // Encode the vector
  Buffer encoded = vector.encode_with_length();
  
  // Create a Value from the encoded data
  VectorType::ConstPtr vector_type(new VectorType(float_type, 0));
  Decoder decoder(encoded.data() + sizeof(int32_t), encoded.size() - sizeof(int32_t), 4);
  Value value(vector_type, decoder);
  
  // Create an iterator
  VectorIterator iterator(&value);
  
  // Should not iterate at all
  EXPECT_FALSE(iterator.next());
}

TEST_F(VectorIteratorTest, SingleElementVector) {
  // Create a single-element vector
  DataType::ConstPtr double_type(new DataType(CASS_VALUE_TYPE_DOUBLE));
  CassandraVector vector(double_type, 1);
  
  // Add one double
  ASSERT_EQ(vector.append(3.14159), CASS_OK);
  
  // Encode the vector
  Buffer encoded = vector.encode_with_length();
  
  // Create a Value from the encoded data
  VectorType::ConstPtr vector_type(new VectorType(double_type, 1));
  Decoder decoder(encoded.data() + sizeof(int32_t), encoded.size() - sizeof(int32_t), 4);
  Value value(vector_type, decoder);
  
  // Create an iterator
  VectorIterator iterator(&value);
  
  // Should iterate exactly once
  EXPECT_TRUE(iterator.next());
  
  const Value* element = iterator.value();
  ASSERT_FALSE(element->is_null());
  ASSERT_EQ(element->value_type(), CASS_VALUE_TYPE_DOUBLE);
  
  cass_double_t actual;
  ASSERT_EQ(cass_value_get_double(element, &actual), CASS_OK);
  EXPECT_DOUBLE_EQ(actual, 3.14159);
  
  // Should not iterate again
  EXPECT_FALSE(iterator.next());
}

TEST_F(VectorIteratorTest, CAPIIteratorFromVector) {
  // Test the C API function cass_iterator_from_vector
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  CassandraVector vector(int_type, 2);
  
  ASSERT_EQ(vector.append(100), CASS_OK);
  ASSERT_EQ(vector.append(200), CASS_OK);
  
  Buffer encoded = vector.encode_with_length();
  
  // Create a CUSTOM type for vector (as vectors are custom types)
  CustomType::ConstPtr custom_type(new CustomType("org.apache.cassandra.db.marshal.VectorType(org.apache.cassandra.db.marshal.Int32Type, 2)"));
  Decoder decoder(encoded.data() + sizeof(int32_t), encoded.size() - sizeof(int32_t), 4);
  Value value(custom_type, decoder);
  
  // Use the C API to create an iterator
  CassIterator* iter = cass_iterator_from_vector(&value);
  ASSERT_NE(iter, nullptr);
  
  // Check iterator type
  EXPECT_EQ(cass_iterator_type(iter), CASS_ITERATOR_TYPE_VECTOR);
  
  // Iterate using C API
  int32_t expected[] = {100, 200};
  int index = 0;
  
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    ASSERT_NE(element, nullptr);
    
    cass_int32_t actual;
    ASSERT_EQ(cass_value_get_int32(element, &actual), CASS_OK);
    EXPECT_EQ(actual, expected[index]);
    index++;
  }
  
  EXPECT_EQ(index, 2);
  
  cass_iterator_free(iter);
}