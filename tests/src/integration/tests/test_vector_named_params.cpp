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
 * Named parameter binding tests for vectors
 */
class VectorNamedParamsTest : public Integration {
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
 * Test basic named parameter binding with vectors
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Named parameters work with vectors
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNamedParamsTest, BasicNamedBinding) {
  CHECK_FAILURE;
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS named_params ("
                   "id int PRIMARY KEY, "
                   "floats vector<float, 3>, "
                   "texts vector<text, 2>)");
  
  // Prepare statement with named parameters
  Prepared prepared = session_.prepare(
      "INSERT INTO named_params (id, floats, texts) VALUES (:id, :float_vec, :text_vec)");
  
  CassStatement* stmt = cass_prepared_bind(prepared.get());
  
  // Bind by name - id
  cass_statement_bind_int32_by_name(stmt, "id", 1);
  
  // Bind by name - float vector
  CassVector* float_vec = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
  cass_vector_append_float(float_vec, 1.0f);
  cass_vector_append_float(float_vec, 2.0f);
  cass_vector_append_float(float_vec, 3.0f);
  cass_statement_bind_vector_by_name(stmt, "float_vec", float_vec);
  cass_vector_free(float_vec);
  
  // Bind by name - text vector
  CassVector* text_vec = cass_vector_new(CASS_VALUE_TYPE_TEXT, 2);
  cass_vector_append_string(text_vec, "hello");
  cass_vector_append_string(text_vec, "world");
  cass_statement_bind_vector_by_name(stmt, "text_vec", text_vec);
  cass_vector_free(text_vec);
  
  // Execute
  CassFuture* future = cass_session_execute(session_.get(), stmt);
  cass_statement_free(stmt);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT * FROM named_params WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
}

/**
 * Test named parameters with multiple vector types
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result All vector types work with named parameters
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNamedParamsTest, MultipleVectorTypes) {
  CHECK_FAILURE;
  
  // Create table with various vector types
  session_.execute("CREATE TABLE IF NOT EXISTS multi_vectors ("
                   "id int PRIMARY KEY, "
                   "ints vector<int, 2>, "
                   "doubles vector<double, 3>, "
                   "booleans vector<boolean, 2>, "
                   "uuids vector<uuid, 2>)");
  
  // Prepare with named parameters
  Prepared prepared = session_.prepare(
      "INSERT INTO multi_vectors (id, ints, doubles, booleans, uuids) "
      "VALUES (:key, :int_vec, :double_vec, :bool_vec, :uuid_vec)");
  
  CassStatement* stmt = cass_prepared_bind(prepared.get());
  
  // Bind all parameters by name
  cass_statement_bind_int32_by_name(stmt, "key", 1);
  
  // Int vector
  CassVector* int_vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
  cass_vector_append_int32(int_vec, 100);
  cass_vector_append_int32(int_vec, 200);
  cass_statement_bind_vector_by_name(stmt, "int_vec", int_vec);
  cass_vector_free(int_vec);
  
  // Double vector
  CassVector* double_vec = cass_vector_new(CASS_VALUE_TYPE_DOUBLE, 3);
  cass_vector_append_double(double_vec, 1.1);
  cass_vector_append_double(double_vec, 2.2);
  cass_vector_append_double(double_vec, 3.3);
  cass_statement_bind_vector_by_name(stmt, "double_vec", double_vec);
  cass_vector_free(double_vec);
  
  // Boolean vector
  CassVector* bool_vec = cass_vector_new(CASS_VALUE_TYPE_BOOLEAN, 2);
  cass_vector_append_bool(bool_vec, cass_true);
  cass_vector_append_bool(bool_vec, cass_false);
  cass_statement_bind_vector_by_name(stmt, "bool_vec", bool_vec);
  cass_vector_free(bool_vec);
  
  // UUID vector
  CassVector* uuid_vec = cass_vector_new(CASS_VALUE_TYPE_UUID, 2);
  CassUuid uuid1, uuid2;
  cass_uuid_from_string("550e8400-e29b-41d4-a716-446655440001", &uuid1);
  cass_uuid_from_string("550e8400-e29b-41d4-a716-446655440002", &uuid2);
  cass_vector_append_uuid(uuid_vec, uuid1);
  cass_vector_append_uuid(uuid_vec, uuid2);
  cass_statement_bind_vector_by_name(stmt, "uuid_vec", uuid_vec);
  cass_vector_free(uuid_vec);
  
  // Execute
  CassFuture* future = cass_session_execute(session_.get(), stmt);
  cass_statement_free(stmt);
  
  CassError rc = cass_future_error_code(future);
  cass_future_free(future);
  ASSERT_EQ(CASS_OK, rc);
  
  // Verify
  Result result = session_.execute("SELECT COUNT(*) FROM multi_vectors");
  ASSERT_EQ(1, result.first_row().column_by_name<BigInteger>("count").value());
}

/**
 * Test partial binding with named parameters
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Can bind vectors while leaving other params null
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNamedParamsTest, PartialBinding) {
  CHECK_FAILURE;
  
  // Create table with nullable columns
  session_.execute("CREATE TABLE IF NOT EXISTS partial_binding ("
                   "id int PRIMARY KEY, "
                   "required_vec vector<int, 2>, "
                   "optional_vec vector<float, 3>)");
  
  // Prepare statement
  Prepared prepared = session_.prepare(
      "INSERT INTO partial_binding (id, required_vec, optional_vec) "
      "VALUES (:id, :req, :opt)");
  
  // Test 1: Bind all parameters
  {
    CassStatement* stmt = cass_prepared_bind(prepared.get());
    cass_statement_bind_int32_by_name(stmt, "id", 1);
    
    CassVector* req_vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
    cass_vector_append_int32(req_vec, 1);
    cass_vector_append_int32(req_vec, 2);
    cass_statement_bind_vector_by_name(stmt, "req", req_vec);
    cass_vector_free(req_vec);
    
    CassVector* opt_vec = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 3);
    cass_vector_append_float(opt_vec, 1.0f);
    cass_vector_append_float(opt_vec, 2.0f);
    cass_vector_append_float(opt_vec, 3.0f);
    cass_statement_bind_vector_by_name(stmt, "opt", opt_vec);
    cass_vector_free(opt_vec);
    
    CassFuture* future = cass_session_execute(session_.get(), stmt);
    cass_statement_free(stmt);
    ASSERT_EQ(CASS_OK, cass_future_error_code(future));
    cass_future_free(future);
  }
  
  // Test 2: Bind only required parameters, leave optional null
  {
    CassStatement* stmt = cass_prepared_bind(prepared.get());
    cass_statement_bind_int32_by_name(stmt, "id", 2);
    
    CassVector* req_vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
    cass_vector_append_int32(req_vec, 3);
    cass_vector_append_int32(req_vec, 4);
    cass_statement_bind_vector_by_name(stmt, "req", req_vec);
    cass_vector_free(req_vec);
    
    // Leave "opt" unbound (null)
    cass_statement_bind_null_by_name(stmt, "opt");
    
    CassFuture* future = cass_session_execute(session_.get(), stmt);
    cass_statement_free(stmt);
    ASSERT_EQ(CASS_OK, cass_future_error_code(future));
    cass_future_free(future);
  }
  
  // Verify both rows
  Result result = session_.execute("SELECT COUNT(*) FROM partial_binding");
  ASSERT_EQ(2, result.first_row().column_by_name<BigInteger>("count").value());
}

/**
 * Test error handling with wrong parameter names
 *
 * @test_category data_types:vector
 * @cassandra_version 5.0.0
 * @expected_result Binding with wrong names fails appropriately
 */
CASSANDRA_INTEGRATION_TEST_F(VectorNamedParamsTest, WrongParameterName) {
  CHECK_FAILURE;
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS wrong_names ("
                   "id int PRIMARY KEY, "
                   "data vector<int, 2>)");
  
  // Prepare statement
  Prepared prepared = session_.prepare(
      "INSERT INTO wrong_names (id, data) VALUES (:id, :data)");
  
  CassStatement* stmt = cass_prepared_bind(prepared.get());
  cass_statement_bind_int32_by_name(stmt, "id", 1);
  
  // Try to bind with wrong parameter name
  CassVector* vec = cass_vector_new(CASS_VALUE_TYPE_INT, 2);
  cass_vector_append_int32(vec, 1);
  cass_vector_append_int32(vec, 2);
  
  // This should return an error code
  CassError bind_result = cass_statement_bind_vector_by_name(stmt, "wrong_name", vec);
  ASSERT_NE(CASS_OK, bind_result);
  
  cass_vector_free(vec);
  cass_statement_free(stmt);
}