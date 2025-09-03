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
 * Vector metadata parsing integration tests
 */
class VectorMetadataTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Create test keyspace
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_metadata "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_metadata");
  }
  
  void TearDown() {
    session_.execute("DROP KEYSPACE IF EXISTS vector_metadata");
    Integration::TearDown();
  }
};

/**
 * Test result metadata parsing for vector columns
 */
CASSANDRA_INTEGRATION_TEST_F(VectorMetadataTest, ResultMetadataWithVectors) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table with various vector columns
  session_.execute("CREATE TABLE IF NOT EXISTS test_vectors ("
                   "id int PRIMARY KEY, "
                   "float_vec vector<float, 3>, "
                   "text_vec vector<text, 2>, "
                   "int_vec vector<int, 4>)");
  
  // Insert a row
  session_.execute("INSERT INTO test_vectors (id, float_vec, text_vec, int_vec) VALUES ("
                   "1, [1.0, 2.0, 3.0], ['hello', 'world'], [1, 2, 3, 4])");
  
  // Query and check metadata
  Result result = session_.execute("SELECT * FROM test_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  // Check column count
  EXPECT_EQ(4ul, result.column_count());
  
  // Check column types
  for (size_t i = 0; i < result.column_count(); i++) {
    const CassDataType* data_type = cass_result_column_data_type(result.get(), i);
    ASSERT_NE(data_type, nullptr);
    
    String name;
    result.column_name(i, &name);
    
    CassValueType value_type = cass_data_type_type(data_type);
    
    TEST_LOG("Column %zu: %s, type=%d", i, name.c_str(), value_type);
    
    if (name == "float_vec" || name == "text_vec" || name == "int_vec") {
      // Should be CUSTOM type (vectors are custom types)
      EXPECT_EQ(value_type, CASS_VALUE_TYPE_CUSTOM) 
        << "Vector column " << name << " should be CUSTOM type";
      
      // For vectors, we should be able to cast to VectorType
      const DataType* dt = reinterpret_cast<const DataType*>(data_type);
      if (dt->value_type() == CASS_VALUE_TYPE_CUSTOM) {
        const CustomType* custom = static_cast<const CustomType*>(dt);
        TEST_LOG("  Custom class: %s", custom->class_name().c_str());
        
        // Try to parse as vector
        VectorType::ConstPtr vec_type = VectorType::from_class_name(custom->class_name());
        if (vec_type) {
          TEST_LOG("  Successfully parsed as vector!");
          TEST_LOG("  Dimension: %zu", vec_type->dimension());
          TEST_LOG("  Element type: %d", vec_type->element_type()->value_type());
          
          // Verify dimensions
          if (name == "float_vec") {
            EXPECT_EQ(vec_type->dimension(), 3ul);
            EXPECT_EQ(vec_type->element_type()->value_type(), CASS_VALUE_TYPE_FLOAT);
          } else if (name == "text_vec") {
            EXPECT_EQ(vec_type->dimension(), 2ul);
            EXPECT_EQ(vec_type->element_type()->value_type(), CASS_VALUE_TYPE_TEXT);
          } else if (name == "int_vec") {
            EXPECT_EQ(vec_type->dimension(), 4ul);
            EXPECT_EQ(vec_type->element_type()->value_type(), CASS_VALUE_TYPE_INT);
          }
        } else {
          FAIL() << "Failed to parse vector type from: " << custom->class_name();
        }
      }
    }
  }
  
  TEST_LOG("Result metadata parsing successful!");
}

/**
 * Test prepared statement metadata for vector parameters
 */
CASSANDRA_INTEGRATION_TEST_F(VectorMetadataTest, PreparedStatementVectorMetadata) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS test_prepared ("
                   "id int PRIMARY KEY, "
                   "vec vector<float, 5>)");
  
  // Prepare statement with vector parameter
  Prepared prepared = session_.prepare("INSERT INTO test_prepared (id, vec) VALUES (?, ?)");
  
  // Get metadata
  const CassPreparedMetadata* metadata = cass_prepared_metadata(prepared.get());
  ASSERT_NE(metadata, nullptr);
  
  size_t param_count = cass_prepared_metadata_parameter_count(metadata);
  EXPECT_EQ(param_count, 2ul);
  
  // Check parameter types
  for (size_t i = 0; i < param_count; i++) {
    const CassDataType* param_type = cass_prepared_metadata_parameter_data_type(metadata, i);
    ASSERT_NE(param_type, nullptr);
    
    CassValueType value_type = cass_data_type_type(param_type);
    
    if (i == 0) {
      // First parameter is int
      EXPECT_EQ(value_type, CASS_VALUE_TYPE_INT);
    } else if (i == 1) {
      // Second parameter is vector<float, 5>
      EXPECT_EQ(value_type, CASS_VALUE_TYPE_CUSTOM);
      
      const DataType* dt = reinterpret_cast<const DataType*>(param_type);
      if (dt->value_type() == CASS_VALUE_TYPE_CUSTOM) {
        const CustomType* custom = static_cast<const CustomType*>(dt);
        
        // Parse as vector
        VectorType::ConstPtr vec_type = VectorType::from_class_name(custom->class_name());
        if (vec_type) {
          EXPECT_EQ(vec_type->dimension(), 5ul);
          EXPECT_EQ(vec_type->element_type()->value_type(), CASS_VALUE_TYPE_FLOAT);
          TEST_LOG("Prepared statement parameter is vector<float, 5>");
        } else {
          FAIL() << "Failed to parse vector parameter type";
        }
      }
    }
  }
  
  TEST_LOG("Prepared statement metadata parsing successful!");
}

/**
 * Test metadata for nested vector types
 */
CASSANDRA_INTEGRATION_TEST_F(VectorMetadataTest, NestedVectorMetadata) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table with nested vector type
  session_.execute("CREATE TABLE IF NOT EXISTS test_nested ("
                   "id int PRIMARY KEY, "
                   "nested_vec vector<frozen<list<int>>, 2>)");
  
  // Insert data
  session_.execute("INSERT INTO test_nested (id, nested_vec) VALUES ("
                   "1, [[1, 2, 3], [4, 5]])");
  
  // Query and check metadata
  Result result = session_.execute("SELECT nested_vec FROM test_nested WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  // Check nested vector metadata
  const CassDataType* data_type = cass_result_column_data_type(result.get(), 0);
  ASSERT_NE(data_type, nullptr);
  
  CassValueType value_type = cass_data_type_type(data_type);
  EXPECT_EQ(value_type, CASS_VALUE_TYPE_CUSTOM);
  
  const DataType* dt = reinterpret_cast<const DataType*>(data_type);
  if (dt->value_type() == CASS_VALUE_TYPE_CUSTOM) {
    const CustomType* custom = static_cast<const CustomType*>(dt);
    TEST_LOG("Nested vector class: %s", custom->class_name().c_str());
    
    // Parse as vector
    VectorType::ConstPtr vec_type = VectorType::from_class_name(custom->class_name());
    if (vec_type) {
      EXPECT_EQ(vec_type->dimension(), 2ul);
      
      // Element should be frozen list
      ASSERT_TRUE(vec_type->element_type());
      EXPECT_EQ(vec_type->element_type()->value_type(), CASS_VALUE_TYPE_LIST);
      
      const CollectionType* list_type = static_cast<const CollectionType*>(vec_type->element_type().get());
      EXPECT_TRUE(list_type->is_frozen());
      
      // List element should be int
      ASSERT_TRUE(list_type->types()[0]);
      EXPECT_EQ(list_type->types()[0]->value_type(), CASS_VALUE_TYPE_INT);
      
      TEST_LOG("Successfully parsed nested vector<frozen<list<int>>>");
    } else {
      FAIL() << "Failed to parse nested vector type";
    }
  }
  
  TEST_LOG("Nested vector metadata parsing successful!");
}

/**
 * Test system_schema.columns for vector columns
 */
CASSANDRA_INTEGRATION_TEST_F(VectorMetadataTest, SystemSchemaVectorColumns) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create table with vectors
  session_.execute("CREATE TABLE IF NOT EXISTS schema_test ("
                   "id int PRIMARY KEY, "
                   "embedding vector<float, 384>)");
  
  // Query system_schema.columns
  Result result = session_.execute(
      "SELECT column_name, type "
      "FROM system_schema.columns "
      "WHERE keyspace_name = 'vector_metadata' "
      "AND table_name = 'schema_test' "
      "AND column_name = 'embedding'");
  
  ASSERT_GT(result.row_count(), 0ul);
  
  Row row = result.first_row();
  
  // Get the type string
  String type_str;
  ASSERT_TRUE(row.get_string(1, &type_str));
  
  TEST_LOG("Vector column type in system_schema: %s", type_str.c_str());
  
  // Should be something like "vector<float, 384>"
  EXPECT_TRUE(type_str.find("vector") != String::npos);
  EXPECT_TRUE(type_str.find("float") != String::npos);
  EXPECT_TRUE(type_str.find("384") != String::npos);
  
  TEST_LOG("System schema query successful!");
}