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
 * Verify we are actually running against Cassandra 5.0+
 * This test queries the system.local table to get the actual Cassandra version
 * and verifies it's 5.0.0 or higher
 */
CASSANDRA_INTEGRATION_TEST_F(Cassandra5BasicTest, VerifyCassandra5Version) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.0);
  
  // Query the actual Cassandra version from system.local
  Result result = session_.execute("SELECT release_version FROM system.local");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  Text version = row.column_by_name<Text>("release_version");
  std::string version_str = version.str();
  
  TEST_LOG("Connected to Cassandra version: " << version_str);
  
  // Parse the version string (format: X.Y.Z or X.Y.Z-SNAPSHOT)
  // We need to verify it's at least 5.0.0
  int major = 0, minor = 0, patch = 0;
  if (sscanf(version_str.c_str(), "%d.%d.%d", &major, &minor, &patch) >= 2) {
    ASSERT_GE(major, 5) << "Expected Cassandra 5.0 or higher, but got " << version_str;
    if (major == 5) {
      ASSERT_GE(minor, 0) << "Expected Cassandra 5.0 or higher, but got " << version_str;
    }
    TEST_LOG("Version check passed: Cassandra " << major << "." << minor << "." << patch << " >= 5.0.0");
  } else {
    FAIL() << "Could not parse Cassandra version: " << version_str;
  }
}