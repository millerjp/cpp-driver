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
#include "collection_iterator.hpp"
#include "data_type.hpp"
#include "decoder.hpp"
#include "value.hpp"

using namespace datastax::internal::core;

class VectorIteratorUnitTest : public testing::Test {
public:
  void SetUp() {}
};

/**
 * Test that VectorIterator properly fails on malformed vector type
 */
TEST_F(VectorIteratorUnitTest, FailsOnMalformedVectorType) {
  // Create a custom type that claims to be a vector but is malformed
  DataType::ConstPtr malformed_type(new CustomType("org.apache.cassandra.db.marshal.VectorType(malformed)"));
  
  // Create some dummy data
  const char dummy_data[] = {0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04};
  Decoder decoder(dummy_data, sizeof(dummy_data), 4);  // Protocol v4
  
  // Create a value with malformed type
  Value malformed_value(malformed_type, decoder);
  
  // Create VectorIterator - should be invalid
  VectorIterator* iterator = new VectorIterator(&malformed_value);
  
  // Verify iterator doesn't iterate (returns false immediately)
  EXPECT_FALSE(iterator->next());
  
  delete iterator;
}

/**
 * Test that VectorIterator fails on non-custom type
 */
TEST_F(VectorIteratorUnitTest, FailsOnNonCustomType) {
  // Create a regular INT type (not custom)
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  
  const char dummy_data[] = {0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04};
  Decoder decoder(dummy_data, sizeof(dummy_data), 4);
  
  Value int_value(int_type, decoder);
  
  // Create VectorIterator on non-vector type
  VectorIterator* iterator = new VectorIterator(&int_value);
  
  // Should fail to iterate
  EXPECT_FALSE(iterator->next());
  
  delete iterator;
}

/**
 * Test that VectorIterator fails on invalid custom class name
 */
TEST_F(VectorIteratorUnitTest, FailsOnInvalidCustomClassName) {
  // Create a custom type with completely wrong class name
  DataType::ConstPtr wrong_type(new CustomType("com.example.InvalidType"));
  
  const char dummy_data[] = {0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04};
  Decoder decoder(dummy_data, sizeof(dummy_data), 4);
  
  Value wrong_value(wrong_type, decoder);
  
  VectorIterator* iterator = new VectorIterator(&wrong_value);
  
  // Should fail to iterate
  EXPECT_FALSE(iterator->next());
  
  delete iterator;
}

/**
 * Test that existing CollectionIterator still works properly
 */
TEST_F(VectorIteratorUnitTest, CollectionIteratorStillWorks) {
  // Create a LIST type
  DataType::ConstPtr element_type(new DataType(CASS_VALUE_TYPE_INT));
  DataType::ConstPtr list_type(new CollectionType(CASS_COLLECTION_TYPE_LIST, element_type, false));
  
  // Create data for list with 2 elements: [1, 2]
  // Format: count(4 bytes) + size1(4 bytes) + value1 + size2(4 bytes) + value2
  const char list_data[] = {
    0x00, 0x00, 0x00, 0x02,  // count = 2
    0x00, 0x00, 0x00, 0x04,  // size1 = 4
    0x00, 0x00, 0x00, 0x01,  // value1 = 1
    0x00, 0x00, 0x00, 0x04,  // size2 = 4
    0x00, 0x00, 0x00, 0x02   // value2 = 2
  };
  
  Decoder decoder(list_data, sizeof(list_data), 4);
  
  // Read count
  int32_t count = 0;
  ASSERT_TRUE(decoder.decode_int32(count));
  EXPECT_EQ(count, 2);
  
  // Create value
  Value list_value(list_type, count, decoder);
  
  // Create CollectionIterator
  CollectionIterator* iterator = new CollectionIterator(&list_value, count);
  
  // Should be able to iterate twice
  EXPECT_TRUE(iterator->next());
  EXPECT_TRUE(iterator->next());
  EXPECT_FALSE(iterator->next()); // No more elements
  
  delete iterator;
}