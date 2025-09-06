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
 * Complex combination tests for vectors with UDTs and collections
 */
class VectorComplexCombinationsTest : public Integration {
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
 * Test vectors containing lists with UDTs: vector<frozen<list<frozen<udt>>>>
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Complex nested type works
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexCombinationsTest, VectorOfListsWithUDT) {
  CHECK_FAILURE;
  
  // Create UDT
  session_.execute("CREATE TYPE IF NOT EXISTS person ("
                   "name text, "
                   "age int)");
  
  // Create table with vector of lists containing UDTs
  session_.execute("CREATE TABLE IF NOT EXISTS vector_list_udt ("
                   "id int PRIMARY KEY, "
                   "people_lists vector<frozen<list<frozen<person>>>, 2>)");
  
  // Insert using CQL
  session_.execute("INSERT INTO vector_list_udt (id, people_lists) VALUES (1, "
                   "[[{name: 'Alice', age: 30}, {name: 'Bob', age: 25}], "
                   "[{name: 'Charlie', age: 35}]])");
  
  // Verify data was inserted
  Result result = session_.execute("SELECT * FROM vector_list_udt WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test vectors containing sets with UDTs: vector<frozen<set<frozen<udt>>>>
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Set of UDTs in vector works
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexCombinationsTest, VectorOfSetsWithUDT) {
  CHECK_FAILURE;
  
  // Create UDT
  session_.execute("CREATE TYPE IF NOT EXISTS product ("
                   "id uuid, "
                   "name text, "
                   "price decimal)");
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS vector_set_udt ("
                   "id int PRIMARY KEY, "
                   "product_sets vector<frozen<set<frozen<product>>>, 3>)");
  
  // Insert using CQL
  session_.execute("INSERT INTO vector_set_udt (id, product_sets) VALUES (1, ["
                   "{" // First set
                   "  {id: 550e8400-e29b-41d4-a716-446655440001, name: 'Item1', price: 10.99}, "
                   "  {id: 550e8400-e29b-41d4-a716-446655440002, name: 'Item2', price: 20.99}"
                   "}, "
                   "{" // Second set  
                   "  {id: 550e8400-e29b-41d4-a716-446655440003, name: 'Item3', price: 30.99}"
                   "}, "
                   "{" // Third set
                   "  {id: 550e8400-e29b-41d4-a716-446655440004, name: 'Item4', price: 40.99}"
                   "}"
                   "])");
  
  // Verify
  Result result = session_.execute("SELECT COUNT(*) FROM vector_set_udt");
  ASSERT_EQ(1, result.first_row().column_by_name<BigInteger>("count").value());
}

/**
 * Test vectors containing maps with UDT values: vector<frozen<map<text, frozen<udt>>>>
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Map with UDT values in vector works
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexCombinationsTest, VectorOfMapsWithUDTValues) {
  CHECK_FAILURE;
  
  // Create UDT
  session_.execute("CREATE TYPE IF NOT EXISTS location ("
                   "lat double, "
                   "lon double, "
                   "name text)");
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS vector_map_udt ("
                   "id int PRIMARY KEY, "
                   "location_maps vector<frozen<map<text, frozen<location>>>, 2>)");
  
  // Insert using CQL
  session_.execute("INSERT INTO vector_map_udt (id, location_maps) VALUES (1, ["
                   "{'home': {lat: 37.7749, lon: -122.4194, name: 'San Francisco'}, "
                   " 'work': {lat: 37.4419, lon: -122.1430, name: 'Palo Alto'}}, "
                   "{'vacation': {lat: 21.3099, lon: -157.8581, name: 'Honolulu'}}"
                   "])");
  
  // Verify
  Result result = session_.execute("SELECT * FROM vector_map_udt WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test vectors containing UDTs with collection fields
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result UDTs with collection fields work in vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexCombinationsTest, VectorOfUDTWithCollections) {
  CHECK_FAILURE;
  
  // Create UDT with collection fields
  session_.execute("CREATE TYPE IF NOT EXISTS user_profile ("
                   "username text, "
                   "tags set<text>, "
                   "attributes map<text, int>)");
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS vector_udt_collections ("
                   "id int PRIMARY KEY, "
                   "profiles vector<frozen<user_profile>, 2>)");
  
  // Insert using CQL
  session_.execute("INSERT INTO vector_udt_collections (id, profiles) VALUES (1, ["
                   "{username: 'alice', tags: {'admin', 'user'}, attributes: {'level': 5}}, "
                   "{username: 'bob', tags: {'user'}, attributes: {'level': 2, 'score': 100}}"
                   "])");
  
  // Verify
  Result result = session_.execute("SELECT * FROM vector_udt_collections WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test unfrozen vs frozen UDTs in vectors
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Both frozen and unfrozen UDTs work in Cassandra 5.0.5
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexCombinationsTest, UnfrozenVsFrozenUDT) {
  CHECK_FAILURE;
  
  // Create UDT
  session_.execute("CREATE TYPE IF NOT EXISTS simple_type (value int)");
  
  // Create table with frozen UDT in vector
  session_.execute("CREATE TABLE IF NOT EXISTS frozen_udt_vector ("
                   "id int PRIMARY KEY, "
                   "data vector<frozen<simple_type>, 2>)");
  
  // Create table with unfrozen UDT in vector (works in Cassandra 5.0.5)
  session_.execute("CREATE TABLE IF NOT EXISTS unfrozen_udt_vector ("
                   "id int PRIMARY KEY, "
                   "data vector<simple_type, 2>)");
  
  // Insert into frozen table
  session_.execute("INSERT INTO frozen_udt_vector (id, data) VALUES (1, "
                   "[{value: 10}, {value: 20}])");
  
  // Insert into unfrozen table  
  session_.execute("INSERT INTO unfrozen_udt_vector (id, data) VALUES (1, "
                   "[{value: 30}, {value: 40}])");
  
  // Verify both work
  Result r1 = session_.execute("SELECT * FROM frozen_udt_vector WHERE id = 1");
  ASSERT_EQ(1ul, r1.row_count());
  
  Result r2 = session_.execute("SELECT * FROM unfrozen_udt_vector WHERE id = 1");
  ASSERT_EQ(1ul, r2.row_count());
}

/**
 * Test unfrozen vs frozen collections in vectors
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0  
 * @expected_result Both frozen and unfrozen collections work in Cassandra 5.0.5
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexCombinationsTest, UnfrozenVsFrozenCollections) {
  CHECK_FAILURE;
  
  // Create table with frozen list in vector
  session_.execute("CREATE TABLE IF NOT EXISTS frozen_list_vector ("
                   "id int PRIMARY KEY, "
                   "data vector<frozen<list<int>>, 2>)");
  
  // Create table with unfrozen list in vector (works in Cassandra 5.0.5)
  session_.execute("CREATE TABLE IF NOT EXISTS unfrozen_list_vector ("
                   "id int PRIMARY KEY, "
                   "data vector<list<int>, 2>)");
  
  // Insert into both
  session_.execute("INSERT INTO frozen_list_vector (id, data) VALUES (1, "
                   "[[1, 2, 3], [4, 5]])");
  
  session_.execute("INSERT INTO unfrozen_list_vector (id, data) VALUES (1, "
                   "[[10, 20], [30, 40, 50]])");
  
  // Verify both work
  Result r1 = session_.execute("SELECT * FROM frozen_list_vector WHERE id = 1");
  ASSERT_EQ(1ul, r1.row_count());
  
  Result r2 = session_.execute("SELECT * FROM unfrozen_list_vector WHERE id = 1");
  ASSERT_EQ(1ul, r2.row_count());
}

/**
 * Test vectors of vectors with UDTs: vector<frozen<vector<frozen<udt>>>>
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Nested vectors with UDTs work
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexCombinationsTest, VectorOfVectorsWithUDT) {
  CHECK_FAILURE;
  
  // Create UDT
  session_.execute("CREATE TYPE IF NOT EXISTS coordinate (x float, y float)");
  
  // Create table with nested vectors of UDTs
  session_.execute("CREATE TABLE IF NOT EXISTS nested_vector_udt ("
                   "id int PRIMARY KEY, "
                   "coordinate_vectors vector<frozen<vector<frozen<coordinate>, 2>>, 3>)");
  
  // Insert using CQL
  session_.execute("INSERT INTO nested_vector_udt (id, coordinate_vectors) VALUES (1, ["
                   "[{x: 1.0, y: 2.0}, {x: 3.0, y: 4.0}], "
                   "[{x: 5.0, y: 6.0}, {x: 7.0, y: 8.0}], "
                   "[{x: 9.0, y: 10.0}, {x: 11.0, y: 12.0}]"
                   "])");
  
  // Verify
  Result result = session_.execute("SELECT * FROM nested_vector_udt WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test maximum nesting depth with UDTs and collections
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Deep nesting works up to Cassandra's limits
 */
CASSANDRA_INTEGRATION_TEST_F(VectorComplexCombinationsTest, MaximumNestingDepth) {
  CHECK_FAILURE;
  
  // Create nested UDTs
  session_.execute("CREATE TYPE IF NOT EXISTS level3 (value int)");
  session_.execute("CREATE TYPE IF NOT EXISTS level2 (data frozen<level3>)");
  session_.execute("CREATE TYPE IF NOT EXISTS level1 (nested frozen<level2>)");
  
  // Create table with deeply nested structure
  session_.execute("CREATE TABLE IF NOT EXISTS deep_nesting ("
                   "id int PRIMARY KEY, "
                   "complex vector<frozen<list<frozen<level1>>>, 2>)");
  
  // Insert using CQL
  session_.execute("INSERT INTO deep_nesting (id, complex) VALUES (1, ["
                   "[{nested: {data: {value: 100}}}], "
                   "[{nested: {data: {value: 200}}}]"
                   "])");
  
  // Verify
  Result result = session_.execute("SELECT COUNT(*) FROM deep_nesting");
  ASSERT_EQ(1, result.first_row().column_by_name<BigInteger>("count").value());
}