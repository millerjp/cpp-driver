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
 * Test that non-vector custom types are not accidentally treated as vectors
 * This ensures our vector detection doesn't break existing custom type handling
 */
class VectorCustomTypeSafetyTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_custom_test WITH replication = "
                     "{'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_custom_test");
  }
};

/**
 * Test that we correctly identify vector custom types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorCustomTypeSafetyTest, IdentifyVectorCustomType) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create a table with a vector column
  session_.execute("CREATE TABLE vector_test (id int PRIMARY KEY, vec vector<float, 3>)");
  
  // Get metadata for the table
  Result metadata_result = session_.execute(
    "SELECT type FROM system_schema.columns "
    "WHERE keyspace_name = 'vector_custom_test' AND table_name = 'vector_test' AND column_name = 'vec'");
  
  ASSERT_EQ(1ul, metadata_result.row_count());
  
  // The type should be a custom type with VectorType class
  const CassValue* type_value = cass_row_get_column(metadata_result.first_row().get(), 0);
  ASSERT_NE(nullptr, type_value);
  
  const char* type_str;
  size_t type_str_len;
  ASSERT_EQ(CASS_OK, cass_value_get_string(type_value, &type_str, &type_str_len));
  
  std::string type_string(type_str, type_str_len);
  
  // In Cassandra 5, vectors might be represented as 'vector<float, 3>' directly
  // rather than as custom types in the metadata
  bool is_vector_notation = (type_string == "vector<float, 3>");
  bool is_custom_type = (type_string.find("org.apache.cassandra.db.marshal.VectorType") != std::string::npos);
  
  // Accept either representation
  ASSERT_TRUE(is_vector_notation || is_custom_type) 
    << "Expected vector type in metadata but got: " << type_string;
    
  if (is_custom_type) {
    ASSERT_NE(std::string::npos, type_string.find("FloatType"));
    ASSERT_NE(std::string::npos, type_string.find(",3"));
  }
}

/**
 * Test that non-vector custom types remain as CUSTOM type
 * This verifies we don't accidentally convert other custom types to vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorCustomTypeSafetyTest, NonVectorCustomTypeHandling) {
  CHECK_FAILURE;
  
  // Create a custom data type (not a vector)
  CassDataType* custom_type = cass_data_type_new(CASS_VALUE_TYPE_CUSTOM);
  ASSERT_EQ(CASS_OK, cass_data_type_set_class_name(custom_type, "com.example.CustomType"));
  
  // Verify it's still a custom type
  ASSERT_EQ(CASS_VALUE_TYPE_CUSTOM, cass_data_type_type(custom_type));
  
  // Get the class name
  const char* class_name;
  size_t class_name_len;
  ASSERT_EQ(CASS_OK, cass_data_type_class_name(custom_type, &class_name, &class_name_len));
  ASSERT_EQ("com.example.CustomType", std::string(class_name, class_name_len));
  
  cass_data_type_free(custom_type);
}

/**
 * Test that vectors are properly identified when retrieved from metadata
 */
CASSANDRA_INTEGRATION_TEST_F(VectorCustomTypeSafetyTest, VectorTypeFromMetadata) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create tables with different vector types
  session_.execute("CREATE TABLE float_vectors (id int PRIMARY KEY, vec vector<float, 3>)");
  session_.execute("CREATE TABLE int_vectors (id int PRIMARY KEY, vec vector<int, 2>)");
  session_.execute("CREATE TABLE text_vectors (id int PRIMARY KEY, vec vector<text, 4>)");
  
  // Insert test data
  session_.execute("INSERT INTO float_vectors (id, vec) VALUES (1, [1.0, 2.0, 3.0])");
  session_.execute("INSERT INTO int_vectors (id, vec) VALUES (1, [10, 20])");
  session_.execute("INSERT INTO text_vectors (id, vec) VALUES (1, ['a', 'b', 'c', 'd'])");
  
  // Retrieve and verify float vectors
  {
    Result result = session_.execute("SELECT vec FROM float_vectors WHERE id = 1");
    const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
    ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
    
    // Verify we can iterate
    CassIterator* iter = cass_iterator_from_vector(vec_value);
    ASSERT_NE(nullptr, iter);
    
    int count = 0;
    while (cass_iterator_next(iter)) {
      const CassValue* element = cass_iterator_get_value(iter);
      ASSERT_EQ(CASS_VALUE_TYPE_FLOAT, cass_value_type(element));
      count++;
    }
    ASSERT_EQ(3, count);
    cass_iterator_free(iter);
  }
  
  // Retrieve and verify int vectors
  {
    Result result = session_.execute("SELECT vec FROM int_vectors WHERE id = 1");
    const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
    ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
    
    CassIterator* iter = cass_iterator_from_vector(vec_value);
    ASSERT_NE(nullptr, iter);
    
    int count = 0;
    while (cass_iterator_next(iter)) {
      const CassValue* element = cass_iterator_get_value(iter);
      ASSERT_EQ(CASS_VALUE_TYPE_INT, cass_value_type(element));
      count++;
    }
    ASSERT_EQ(2, count);
    cass_iterator_free(iter);
  }
  
  // Retrieve and verify text vectors
  {
    Result result = session_.execute("SELECT vec FROM text_vectors WHERE id = 1");
    const CassValue* vec_value = cass_row_get_column(result.first_row().get(), 0);
    ASSERT_EQ(CASS_VALUE_TYPE_VECTOR, cass_value_type(vec_value));
    
    CassIterator* iter = cass_iterator_from_vector(vec_value);
    ASSERT_NE(nullptr, iter);
    
    int count = 0;
    while (cass_iterator_next(iter)) {
      const CassValue* element = cass_iterator_get_value(iter);
      ASSERT_EQ(CASS_VALUE_TYPE_TEXT, cass_value_type(element));
      count++;
    }
    ASSERT_EQ(4, count);
    cass_iterator_free(iter);
  }
}

/**
 * Test that malformed vector custom types are handled gracefully
 */
CASSANDRA_INTEGRATION_TEST_F(VectorCustomTypeSafetyTest, MalformedVectorCustomTypes) {
  CHECK_FAILURE;
  
  // Create custom types with vector-like names but wrong format
  std::vector<std::string> malformed_classes = {
    "org.apache.cassandra.db.marshal.VectorType",  // Missing parameters
    "org.apache.cassandra.db.marshal.VectorType()",  // Empty parameters
    "org.apache.cassandra.db.marshal.VectorType(FloatType)",  // Missing dimension
    "org.apache.cassandra.db.marshal.VectorType(NotAType,3)",  // Invalid element type
    "org.apache.cassandra.db.marshal.VectorType(FloatType,abc)",  // Non-numeric dimension
    "com.other.VectorType(FloatType,3)"  // Wrong package
  };
  
  for (const auto& class_name : malformed_classes) {
    CassDataType* custom_type = cass_data_type_new(CASS_VALUE_TYPE_CUSTOM);
    ASSERT_EQ(CASS_OK, cass_data_type_set_class_name(custom_type, class_name.c_str()));
    
    // These should remain as custom types, not converted to vector
    ASSERT_EQ(CASS_VALUE_TYPE_CUSTOM, cass_data_type_type(custom_type));
    
    cass_data_type_free(custom_type);
  }
}

/**
 * Test that DSE custom types are not affected by vector handling
 */
CASSANDRA_INTEGRATION_TEST_F(VectorCustomTypeSafetyTest, DSECustomTypesUnaffected) {
  CHECK_FAILURE;
  
  // Common DSE custom type class names
  std::vector<std::string> dse_custom_types = {
    "org.apache.cassandra.db.marshal.DateRangeType",
    "org.apache.cassandra.db.marshal.PointType",
    "org.apache.cassandra.db.marshal.LineStringType",
    "org.apache.cassandra.db.marshal.PolygonType",
    "com.datastax.bdp.db.marshal.DateRangeType",
    "org.apache.cassandra.dht.Murmur3Partitioner"
  };
  
  for (const auto& class_name : dse_custom_types) {
    CassDataType* custom_type = cass_data_type_new(CASS_VALUE_TYPE_CUSTOM);
    ASSERT_EQ(CASS_OK, cass_data_type_set_class_name(custom_type, class_name.c_str()));
    
    // Should remain as custom type
    ASSERT_EQ(CASS_VALUE_TYPE_CUSTOM, cass_data_type_type(custom_type));
    
    // Class name should be preserved
    const char* retrieved_name;
    size_t retrieved_name_len;
    ASSERT_EQ(CASS_OK, cass_data_type_class_name(custom_type, &retrieved_name, &retrieved_name_len));
    ASSERT_EQ(class_name, std::string(retrieved_name, retrieved_name_len));
    
    cass_data_type_free(custom_type);
  }
}