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
 * Test vector dimension limits
 */
TEST(VectorLimitsTest, ZeroDimensionRejected) {
  // Zero dimension should fail
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 0);
  EXPECT_EQ(NULL, vector);
}

TEST(VectorLimitsTest, MaxDimensionAccepted) {
  // Maximum dimension (8192) should succeed - Cassandra 5.0 limit
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 8192);
  EXPECT_NE(nullptr, vector);
  if (vector) {
    cass_vector_free(vector);
  }
}

TEST(VectorLimitsTest, OverMaxDimensionRejected) {
  // Over maximum dimension should fail
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 8193);
  EXPECT_EQ(NULL, vector);
  
  // Way over maximum should also fail
  vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, SIZE_MAX);
  EXPECT_EQ(NULL, vector);
}

TEST(VectorLimitsTest, CommonDimensionsAccepted) {
  // Test common embedding dimensions
  size_t common_dimensions[] = {2, 3, 128, 256, 512, 768, 1024, 1536, 3072};
  
  for (size_t i = 0; i < sizeof(common_dimensions)/sizeof(common_dimensions[0]); i++) {
    size_t dim = common_dimensions[i];
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, dim);
    EXPECT_NE(nullptr, vector) << "Failed to create vector with dimension " << dim;
    if (vector) {
      cass_vector_free(vector);
    }
  }
}

TEST(VectorLimitsTest, UnsupportedTypeRejected) {
  // Test that unsupported types are rejected even with valid dimension
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_CUSTOM, 3);
  EXPECT_EQ(NULL, vector);
  
  vector = cass_vector_new(CASS_VALUE_TYPE_LIST, 3);
  EXPECT_EQ(NULL, vector);
  
  vector = cass_vector_new(CASS_VALUE_TYPE_MAP, 3);
  EXPECT_EQ(NULL, vector);
  
  vector = cass_vector_new(CASS_VALUE_TYPE_SET, 3);
  EXPECT_EQ(NULL, vector);
}