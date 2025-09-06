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
 * Simple UDT vector tests that verify basic functionality
 */
class VectorUDTSimpleTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Vectors require Cassandra 5.0+
    if (!Options::is_cassandra() || server_version_ < "5.0.0") {
      SKIP_TEST("Vector types require Cassandra 5.0+");
    }
    
    session_.execute(
        format_string("CREATE KEYSPACE IF NOT EXISTS %s "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}", 
                     keyspace_name_.c_str()));
    session_.execute("USE " + keyspace_name_);
  }
};

/**
 * Test that UDT vectors can be created in schema
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Table with UDT vector is created successfully
 */
CASSANDRA_INTEGRATION_TEST_F(VectorUDTSimpleTest, CreateUDTVectorTable) {
  CHECK_FAILURE;
  
  // Create UDT
  session_.execute("CREATE TYPE IF NOT EXISTS coordinates ("
                   "x double, "
                   "y double, "
                   "z double)");
  
  // Create table with UDT vector - this tests schema validation
  session_.execute("CREATE TABLE IF NOT EXISTS udt_vector_table ("
                   "id int PRIMARY KEY, "
                   "positions vector<frozen<coordinates>, 3>)");
  
  // Verify table was created
  Result result = session_.execute("SELECT * FROM udt_vector_table LIMIT 1");
  ASSERT_EQ(0ul, result.row_count()); // Empty table
  ASSERT_EQ(2ul, result.column_count()); // id and positions columns
}

/**
 * Test inserting UDT vectors with CQL
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result UDT vectors can be inserted and retrieved via CQL
 */
CASSANDRA_INTEGRATION_TEST_F(VectorUDTSimpleTest, InsertUDTVectorCQL) {
  CHECK_FAILURE;
  
  // Create UDT and table
  session_.execute("CREATE TYPE IF NOT EXISTS point2d (x float, y float)");
  session_.execute("CREATE TABLE IF NOT EXISTS points_table ("
                   "id int PRIMARY KEY, "
                   "points vector<frozen<point2d>, 2>)");
  
  // Insert data using CQL literal
  session_.execute("INSERT INTO points_table (id, points) VALUES (1, "
                   "[{x: 1.0, y: 2.0}, {x: 3.0, y: 4.0}])");
  
  // Query and verify
  Result result = session_.execute("SELECT * FROM points_table WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  // Verify we can retrieve the row
  Row row = result.first_row();
  ASSERT_EQ(1, row.column_by_name<Integer>("id").value());
}

/**
 * Test multiple UDT types in vectors
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Different UDT types work in vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorUDTSimpleTest, MultipleUDTTypes) {
  CHECK_FAILURE;
  
  // Create multiple UDT types
  session_.execute("CREATE TYPE IF NOT EXISTS color (r int, g int, b int)");
  session_.execute("CREATE TYPE IF NOT EXISTS dimension (width double, height double)");
  
  // Create table with multiple vector columns of different UDT types
  session_.execute("CREATE TABLE IF NOT EXISTS multi_udt_vectors ("
                   "id int PRIMARY KEY, "
                   "colors vector<frozen<color>, 3>, "
                   "sizes vector<frozen<dimension>, 2>)");
  
  // Insert using CQL
  session_.execute("INSERT INTO multi_udt_vectors (id, colors, sizes) VALUES (1, "
                   "[{r: 255, g: 0, b: 0}, {r: 0, g: 255, b: 0}, {r: 0, g: 0, b: 255}], "
                   "[{width: 100.0, height: 200.0}, {width: 300.0, height: 400.0}])");
  
  // Verify data exists
  Result result = session_.execute("SELECT COUNT(*) FROM multi_udt_vectors");
  ASSERT_EQ(1, result.first_row().column_by_name<BigInteger>("count").value());
}

/**
 * Test UDT vector dimension limits
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result UDT vectors respect dimension constraints
 */
CASSANDRA_INTEGRATION_TEST_F(VectorUDTSimpleTest, UDTVectorDimensions) {
  CHECK_FAILURE;
  
  // Create simple UDT
  session_.execute("CREATE TYPE IF NOT EXISTS item (name text, value int)");
  
  // Test creating vectors with different dimensions
  session_.execute("CREATE TABLE IF NOT EXISTS dim_test_1 ("
                   "id int PRIMARY KEY, "
                   "items vector<frozen<item>, 1>)");
  
  session_.execute("CREATE TABLE IF NOT EXISTS dim_test_10 ("
                   "id int PRIMARY KEY, "
                   "items vector<frozen<item>, 10>)");
  
  session_.execute("CREATE TABLE IF NOT EXISTS dim_test_100 ("
                   "id int PRIMARY KEY, "
                   "items vector<frozen<item>, 100>)");
  
  // Verify all tables were created
  Result r1 = session_.execute("SELECT * FROM dim_test_1 LIMIT 1");
  ASSERT_EQ(0ul, r1.row_count());
  
  Result r2 = session_.execute("SELECT * FROM dim_test_10 LIMIT 1");
  ASSERT_EQ(0ul, r2.row_count());
  
  Result r3 = session_.execute("SELECT * FROM dim_test_100 LIMIT 1");
  ASSERT_EQ(0ul, r3.row_count());
}