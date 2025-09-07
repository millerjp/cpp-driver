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
#include <vector>

/**
 * Variable-length vector type integration tests
 */
class VectorVariableLengthTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Create test keyspace
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_varlen "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_varlen");
  }
  
  void TearDown() {
    session_.execute("DROP KEYSPACE IF EXISTS vector_varlen");
    Integration::TearDown();
  }
};

/**
 * Test vector<text> round-trip
 */
CASSANDRA_INTEGRATION_TEST_F(VectorVariableLengthTest, TextVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table with text vector
  session_.execute("CREATE TABLE IF NOT EXISTS text_vectors ("
                   "id int PRIMARY KEY, "
                   "vec vector<text, 3>)");
  
  // Create vector with text values
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TEXT, 3);
  ASSERT_NE(vector, nullptr);
  
  // Add text values
  ASSERT_EQ(cass_vector_append_string(vector, "hello"), CASS_OK);
  ASSERT_EQ(cass_vector_append_string(vector, "world"), CASS_OK);
  ASSERT_EQ(cass_vector_append_string(vector, "test"), CASS_OK);
  
  // Insert using prepared statement
  Prepared prepared = session_.prepare("INSERT INTO text_vectors (id, vec) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  session_.execute(stmt);
  cass_vector_free(vector);
  
  // Query back and verify
  Result result = session_.execute("SELECT vec FROM text_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  // Create iterator
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  
  // If iterator creation failed, log why
  if (iter == nullptr) {
    TEST_LOG("ERROR: Failed to create iterator for text vector");
    TEST_LOG("This indicates VectorType parsing failed for text type");
    FAIL() << "Iterator creation failed - check VectorType::from_class_name()";
  }
  
  std::vector<std::string> values;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    const char* str;
    size_t len;
    ASSERT_EQ(cass_value_get_string(element, &str, &len), CASS_OK);
    values.push_back(std::string(str, len));
  }
  cass_iterator_free(iter);
  
  // Verify values
  ASSERT_EQ(values.size(), 3ul);
  EXPECT_EQ(values[0], "hello");
  EXPECT_EQ(values[1], "world");
  EXPECT_EQ(values[2], "test");
  
  TEST_LOG("Text vector round-trip successful!");
}

/**
 * Test vector<blob> round-trip  
 */
CASSANDRA_INTEGRATION_TEST_F(VectorVariableLengthTest, BlobVector) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table with blob vector
  session_.execute("CREATE TABLE IF NOT EXISTS blob_vectors ("
                   "id int PRIMARY KEY, "
                   "vec vector<blob, 2>)");
  
  // Create vector with blob values
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_BLOB, 2);
  ASSERT_NE(vector, nullptr);
  
  // Add blob values
  const cass_byte_t blob1[] = {0x01, 0x02, 0x03, 0x04};
  const cass_byte_t blob2[] = {0x05, 0x06, 0x07, 0x08, 0x09};
  ASSERT_EQ(cass_vector_append_bytes(vector, blob1, sizeof(blob1)), CASS_OK);
  ASSERT_EQ(cass_vector_append_bytes(vector, blob2, sizeof(blob2)), CASS_OK);
  
  // Insert
  Prepared prepared = session_.prepare("INSERT INTO blob_vectors (id, vec) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  session_.execute(stmt);
  cass_vector_free(vector);
  
  // Query back
  Result result = session_.execute("SELECT vec FROM blob_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  // Create iterator
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  
  if (iter == nullptr) {
    TEST_LOG("ERROR: Failed to create iterator for blob vector");
    TEST_LOG("This indicates VectorType parsing failed for blob type");
    FAIL() << "Iterator creation failed - check VectorType::from_class_name()";
  }
  
  std::vector<std::vector<cass_byte_t>> values;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    const cass_byte_t* bytes;
    size_t len;
    ASSERT_EQ(cass_value_get_bytes(element, &bytes, &len), CASS_OK);
    values.push_back(std::vector<cass_byte_t>(bytes, bytes + len));
  }
  cass_iterator_free(iter);
  
  // Verify values
  ASSERT_EQ(values.size(), 2ul);
  ASSERT_EQ(values[0].size(), sizeof(blob1));
  ASSERT_EQ(values[1].size(), sizeof(blob2));
  
  for (size_t i = 0; i < sizeof(blob1); i++) {
    EXPECT_EQ(values[0][i], blob1[i]);
  }
  for (size_t i = 0; i < sizeof(blob2); i++) {
    EXPECT_EQ(values[1][i], blob2[i]);
  }
  
  TEST_LOG("Blob vector round-trip successful!");
}

/**
 * Test empty string in text vector
 */
CASSANDRA_INTEGRATION_TEST_F(VectorVariableLengthTest, TextVectorEmptyString) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table
  session_.execute("CREATE TABLE IF NOT EXISTS text_empty_vectors ("
                   "id int PRIMARY KEY, "
                   "vec vector<text, 2>)");
  
  // Create vector with empty string
  CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_TEXT, 2);
  ASSERT_NE(vector, nullptr);
  
  ASSERT_EQ(cass_vector_append_string(vector, ""), CASS_OK);  // Empty string
  ASSERT_EQ(cass_vector_append_string(vector, "non-empty"), CASS_OK);
  
  // Insert
  Prepared prepared = session_.prepare("INSERT INTO text_empty_vectors (id, vec) VALUES (?, ?)");
  Statement stmt = prepared.bind();
  stmt.bind<Integer>(0, Integer(1));
  
  CassStatement* raw_stmt = stmt.get();
  ASSERT_EQ(cass_statement_bind_vector(raw_stmt, 1, vector), CASS_OK);
  
  session_.execute(stmt);
  cass_vector_free(vector);
  
  // Query back
  Result result = session_.execute("SELECT vec FROM text_empty_vectors WHERE id = 1");
  ASSERT_EQ(1ul, result.row_count());
  
  Row row = result.first_row();
  const CassValue* vec_value = cass_row_get_column(row.get(), 0);
  ASSERT_NE(vec_value, nullptr);
  
  CassIterator* iter = cass_iterator_from_vector(vec_value);
  if (iter == nullptr) {
    TEST_LOG("ERROR: Failed to create iterator for text vector with empty string");
    FAIL();
  }
  
  std::vector<std::string> values;
  while (cass_iterator_next(iter)) {
    const CassValue* element = cass_iterator_get_value(iter);
    const char* str;
    size_t len;
    ASSERT_EQ(cass_value_get_string(element, &str, &len), CASS_OK);
    values.push_back(std::string(str, len));
  }
  cass_iterator_free(iter);
  
  // Verify
  ASSERT_EQ(values.size(), 2ul);
  EXPECT_EQ(values[0], "");  // Empty string preserved
  EXPECT_EQ(values[1], "non-empty");
  
  TEST_LOG("Text vector with empty string successful!");
}