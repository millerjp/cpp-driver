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

/**
 * Test that cass_vector_new() properly rejects types that require subtypes
 */
TEST(VectorApiValidationTest, RejectComplexTypes) {
  // Test that LIST type is rejected (needs inner type)
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_LIST, 2);
    EXPECT_EQ(vec, nullptr) << "cass_vector_new should reject LIST type";
  }
  
  // Test that SET type is rejected (needs inner type)
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_SET, 2);
    EXPECT_EQ(vec, nullptr) << "cass_vector_new should reject SET type";
  }
  
  // Test that MAP type is rejected (needs key and value types)
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_MAP, 2);
    EXPECT_EQ(vec, nullptr) << "cass_vector_new should reject MAP type";
  }
  
  // Test that TUPLE type is rejected (needs field types)
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_TUPLE, 2);
    EXPECT_EQ(vec, nullptr) << "cass_vector_new should reject TUPLE type";
  }
  
  // Test that UDT type is rejected (needs field definitions)
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_UDT, 2);
    EXPECT_EQ(vec, nullptr) << "cass_vector_new should reject UDT type";
  }
  
  // Test that CUSTOM type is rejected (needs class name)
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_CUSTOM, 2);
    EXPECT_EQ(vec, nullptr) << "cass_vector_new should reject CUSTOM type";
  }
}

/**
 * Test that cass_vector_new() accepts primitive types
 */
TEST(VectorApiValidationTest, AcceptPrimitiveTypes) {
  // Test various primitive types that should work
  struct {
    CassValueType type;
    const char* name;
  } test_cases[] = {
    {CASS_VALUE_TYPE_INT, "INT"},
    {CASS_VALUE_TYPE_BIGINT, "BIGINT"},
    {CASS_VALUE_TYPE_FLOAT, "FLOAT"},
    {CASS_VALUE_TYPE_DOUBLE, "DOUBLE"},
    {CASS_VALUE_TYPE_BOOLEAN, "BOOLEAN"},
    {CASS_VALUE_TYPE_TEXT, "TEXT"},
    {CASS_VALUE_TYPE_VARCHAR, "VARCHAR"},
    {CASS_VALUE_TYPE_ASCII, "ASCII"},
    {CASS_VALUE_TYPE_BLOB, "BLOB"},
    {CASS_VALUE_TYPE_UUID, "UUID"},
    {CASS_VALUE_TYPE_TIMEUUID, "TIMEUUID"},
    {CASS_VALUE_TYPE_INET, "INET"},
    {CASS_VALUE_TYPE_DATE, "DATE"},
    {CASS_VALUE_TYPE_TIME, "TIME"},
    {CASS_VALUE_TYPE_TIMESTAMP, "TIMESTAMP"},
    {CASS_VALUE_TYPE_DECIMAL, "DECIMAL"},
    {CASS_VALUE_TYPE_VARINT, "VARINT"},
    {CASS_VALUE_TYPE_DURATION, "DURATION"},
    {CASS_VALUE_TYPE_SMALL_INT, "SMALL_INT"},
    {CASS_VALUE_TYPE_TINY_INT, "TINY_INT"}
  };
  
  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
    CassVector* vec = cass_vector_new(test_cases[i].type, 2);
    EXPECT_NE(vec, nullptr) << "cass_vector_new should accept " << test_cases[i].name << " type";
    if (vec) {
      cass_vector_free(vec);
    }
  }
}

/**
 * Test dimension bounds
 */
TEST(VectorApiValidationTest, DimensionBounds) {
  // Test dimension 0 is rejected
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_INT, 0);
    EXPECT_EQ(vec, nullptr) << "cass_vector_new should reject dimension 0";
  }
  
  // Test dimension > 8192 is rejected
  {
    CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_INT, 8193);
    EXPECT_EQ(vec, nullptr) << "cass_vector_new should reject dimension > 8192";
  }
  
  // Test valid dimensions
  {
    CassVector* vec1 = cass_vector_new(CASS_VALUE_TYPE_INT, 1);
    EXPECT_NE(vec1, nullptr) << "cass_vector_new should accept dimension 1";
    if (vec1) cass_vector_free(vec1);
    
    CassVector* vec2 = cass_vector_new(CASS_VALUE_TYPE_INT, 8192);
    EXPECT_NE(vec2, nullptr) << "cass_vector_new should accept dimension 8192";
    if (vec2) cass_vector_free(vec2);
  }
}