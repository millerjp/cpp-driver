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
 * Basic test to verify Cassandra 5.0+ support
 * This test ensures we can connect and perform basic operations with Cassandra 5.0
 */
class Cassandra5BasicTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
  }
};

/**
 * Test basic connection and query execution
 */
CASSANDRA_INTEGRATION_TEST_F(Cassandra5BasicTest, ConnectAndQuery) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Create a test keyspace
  session_.execute("CREATE KEYSPACE IF NOT EXISTS cassandra5_test "
                   "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
  
  // Use the keyspace
  session_.execute("USE cassandra5_test");
  
  // Create a simple table
  session_.execute("CREATE TABLE IF NOT EXISTS test_table ("
                   "id int PRIMARY KEY, "
                   "value text)");
  
  // Insert data
  session_.execute("INSERT INTO test_table (id, value) VALUES (1, 'test_value')");
  session_.execute("INSERT INTO test_table (id, value) VALUES (2, 'another_value')");
  
  // Query data
  Result result = session_.execute("SELECT * FROM test_table WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  ASSERT_EQ(1, row.column_by_name<Integer>("id").value());
  ASSERT_EQ("test_value", row.column_by_name<Text>("value").str());
  
  // Clean up
  session_.execute("DROP KEYSPACE cassandra5_test");
}

/**
 * Test that we can handle Cassandra 5.0 specific features
 * This is a placeholder for future vector tests
 */
CASSANDRA_INTEGRATION_TEST_F(Cassandra5BasicTest, Cassandra5Features) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Future: Add vector type tests here once implemented
  TEST_LOG("Cassandra 5.0+ confirmed, ready for vector type implementation");
}