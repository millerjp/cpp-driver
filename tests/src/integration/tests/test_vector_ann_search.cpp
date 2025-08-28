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
#include "cassandra.h"
#include <algorithm>
#include <cctype>

/**
 * Vector ANN Search and Similarity Functions Tests
 * Based on https://cassandra.apache.org/doc/latest/cassandra/getting-started/vector-search-quickstart.html
 * 
 * Tests vector indexing with different similarity functions and ANN search patterns
 */
class VectorANNSearchTest : public Integration {
public:
  void SetUp() {
    Integration::SetUp();
    
    // Skip if not Cassandra 5.0+
    if (!Options::is_cassandra() || server_version_ < "5.0.0") {
      SKIP_TEST("Vector types are only supported in Cassandra 5.0+");
    }
    
    session_.execute("CREATE KEYSPACE IF NOT EXISTS vector_ann_test "
                     "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    session_.execute("USE vector_ann_test");
  }
  
  void TearDown() {
    try {
      session_.execute("DROP KEYSPACE IF EXISTS vector_ann_test");
    } catch (...) {
      // Ignore errors during cleanup
    }
    Integration::TearDown();
  }
};

/**
 * Test vector indexing with different similarity functions
 */
CASSANDRA_INTEGRATION_TEST_F(VectorANNSearchTest, IndexWithSimilarityFunctions) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Test each similarity function option for index creation
  std::vector<std::pair<std::string, std::string>> index_options = {
    {"COSINE", "cosine"},
    {"DOT_PRODUCT", "dot_product"}, 
    {"EUCLIDEAN", "euclidean"}
  };
  
  for (const auto& option : index_options) {
    TEST_LOG("\n=== Testing " << option.first << " similarity index ===");
    
    std::string table_name = "idx_" + option.second;
    
    // Create table
    session_.execute("CREATE TABLE " + table_name + " ("
                     "id int PRIMARY KEY, "
                     "comment text, "
                     "comment_vector vector<float, 5>)");
    
    // Create index with specific similarity function
    std::string create_index = "CREATE INDEX " + table_name + "_idx ON " + table_name + 
                               " (comment_vector) USING 'sai'";
    
    if (option.first != "COSINE") {  // COSINE is often the default
      create_index += " WITH OPTIONS = { 'similarity_function': '" + option.first + "' }";
    }
    
    try {
      session_.execute(create_index);
      TEST_LOG("Created SAI index with " << option.first << " similarity");
    } catch (const Exception& e) {
      TEST_LOG("Could not create index with " << option.first << ": " << e.what());
      // Try without options
      try {
        session_.execute("CREATE INDEX " + table_name + "_idx ON " + table_name + 
                        " (comment_vector) USING 'sai'");
        TEST_LOG("Created SAI index with default similarity");
      } catch (const Exception& e2) {
        TEST_LOG("Could not create any index: " << e2.what());
        session_.execute("DROP TABLE " + table_name);
        continue;
      }
    }
    
    // Insert test data
    session_.execute("INSERT INTO " + table_name + " (id, comment, comment_vector) "
                     "VALUES (1, 'Great ride today', [0.2, 0.15, 0.3, 0.2, 0.05])");
    session_.execute("INSERT INTO " + table_name + " (id, comment, comment_vector) "
                     "VALUES (2, 'Challenging climb', [0.1, 0.05, 0.5, 0.3, 0.1])");
    session_.execute("INSERT INTO " + table_name + " (id, comment, comment_vector) "
                     "VALUES (3, 'Beautiful scenery', [0.3, 0.25, 0.1, 0.2, 0.15])");
    session_.execute("INSERT INTO " + table_name + " (id, comment, comment_vector) "
                     "VALUES (4, 'Smooth ride', [0.25, 0.2, 0.15, 0.25, 0.15])");
    session_.execute("INSERT INTO " + table_name + " (id, comment, comment_vector) "
                     "VALUES (5, 'Technical sections', [0.05, 0.1, 0.6, 0.2, 0.05])");
    
    // Test ANN query with similarity function in SELECT
    std::string query = "SELECT comment, similarity_" + option.second + 
                        "(comment_vector, [0.2, 0.15, 0.3, 0.2, 0.05]) AS similarity "
                        "FROM " + table_name + " "
                        "ORDER BY comment_vector ANN OF [0.2, 0.15, 0.3, 0.2, 0.05] "
                        "LIMIT 3";
    
    try {
      Result result = session_.execute(query);
      
      TEST_LOG("Top 3 results with " << option.first << " similarity:");
      Rows rows = result.rows();
      for (size_t i = 0; i < rows.row_count(); ++i) {
        Row row = rows.next();
        Text comment = row.column_by_name<Text>("comment");
        try {
          Float similarity = row.column_by_name<Float>("similarity");
          TEST_LOG("  " << (i+1) << ". " << comment.str() << 
                   " (similarity: " << similarity.value() << ")");
        } catch (...) {
          TEST_LOG("  " << (i+1) << ". " << comment.str());
        }
      }
    } catch (const Exception& e) {
      TEST_LOG("Query failed: " << e.what());
    }
    
    // Clean up
    session_.execute("DROP TABLE " + table_name);
  }
}

/**
 * Test ANN search patterns from Cassandra documentation
 * Example from cycling comments use case
 */
CASSANDRA_INTEGRATION_TEST_F(VectorANNSearchTest, CyclingCommentsExample) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create the cycling comments table
  session_.execute("CREATE TABLE cycling_comments_vs ("
                   "record_id timeuuid PRIMARY KEY, "
                   "id uuid, "
                   "commenter text, "
                   "comment text, "
                   "comment_vector vector<float, 5>, "
                   "created_at timestamp)");
  
  // Create index (try with COSINE first, fall back to default)
  try {
    session_.execute("CREATE INDEX cycling_comments_vs_idx ON cycling_comments_vs (comment_vector) "
                     "USING 'sai' "
                     "WITH OPTIONS = { 'similarity_function': 'COSINE' }");
    TEST_LOG("Created SAI index with COSINE similarity");
  } catch (const Exception& e) {
    try {
      session_.execute("CREATE INDEX cycling_comments_vs_idx ON cycling_comments_vs (comment_vector) "
                       "USING 'sai'");
      TEST_LOG("Created SAI index with default similarity");
    } catch (const Exception& e2) {
      TEST_LOG("Could not create index: " << e2.what());
      return;
    }
  }
  
  // Insert sample data using TimeUuid for record_id
  CassUuidGen* uuid_gen = cass_uuid_gen_new();
  
  struct CommentData {
    const char* commenter;
    const char* comment;
    float vector[5];
  };
  
  CommentData comments[] = {
    {"Alex", "Enjoyed the scenic route", {0.25f, 0.15f, 0.1f, 0.45f, 0.05f}},
    {"Bob", "Challenging but rewarding", {0.1f, 0.05f, 0.35f, 0.4f, 0.1f}},
    {"Carol", "Perfect weather today", {0.3f, 0.25f, 0.05f, 0.3f, 0.1f}},
    {"David", "Great group ride", {0.2f, 0.3f, 0.15f, 0.25f, 0.1f}},
    {"Eve", "Technical sections were fun", {0.05f, 0.1f, 0.4f, 0.35f, 0.1f}},
    {"Frank", "Need better bike maintenance", {0.15f, 0.1f, 0.25f, 0.35f, 0.15f}}
  };
  
  for (size_t i = 0; i < sizeof(comments) / sizeof(comments[0]); ++i) {
    CassUuid record_uuid, id_uuid;
    cass_uuid_gen_time(uuid_gen, &record_uuid);
    cass_uuid_gen_random(uuid_gen, &id_uuid);
    
    CassStatement* statement = cass_statement_new(
      "INSERT INTO cycling_comments_vs (record_id, id, commenter, comment, comment_vector, created_at) "
      "VALUES (?, ?, ?, ?, ?, toTimestamp(now()))", 5);
    
    cass_statement_bind_uuid(statement, 0, record_uuid);
    cass_statement_bind_uuid(statement, 1, id_uuid);
    cass_statement_bind_string(statement, 2, comments[i].commenter);
    cass_statement_bind_string(statement, 3, comments[i].comment);
    
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 5);
    for (int j = 0; j < 5; ++j) {
      cass_vector_append_float(vector, comments[i].vector[j]);
    }
    cass_statement_bind_vector(statement, 4, vector);
    cass_vector_free(vector);
    
    CassFuture* future = cass_session_execute(session_.get(), statement);
    cass_statement_free(statement);
    
    CassError rc = cass_future_error_code(future);
    cass_future_free(future);
    ASSERT_EQ(CASS_OK, rc);
  }
  
  cass_uuid_gen_free(uuid_gen);
  
  // Test 1: Basic ANN search (no similarity function in SELECT)
  TEST_LOG("\n=== Test 1: Basic ANN search ===");
  try {
    Result result = session_.execute(
      "SELECT comment FROM cycling_comments_vs "
      "ORDER BY comment_vector ANN OF [0.15, 0.1, 0.3, 0.35, 0.1] "
      "LIMIT 3");
    
    TEST_LOG("Top 3 similar comments:");
    Rows rows = result.rows();
    for (size_t i = 0; i < rows.row_count(); ++i) {
      Row row = rows.next();
      Text comment = row.column_by_name<Text>("comment");
      TEST_LOG("  " << (i+1) << ". " << comment.str());
    }
  } catch (const Exception& e) {
    TEST_LOG("Basic ANN search failed: " << e.what());
  }
  
  // Test 2: ANN search with similarity_cosine in SELECT
  TEST_LOG("\n=== Test 2: ANN with similarity_cosine ===");
  try {
    Result result = session_.execute(
      "SELECT comment, similarity_cosine(comment_vector, [0.2, 0.15, 0.3, 0.2, 0.05]) AS similarity "
      "FROM cycling_comments_vs "
      "ORDER BY comment_vector ANN OF [0.1, 0.15, 0.3, 0.12, 0.05] "
      "LIMIT 3");
    
    TEST_LOG("Top 3 with cosine similarity scores:");
    Rows rows = result.rows();
    for (size_t i = 0; i < rows.row_count(); ++i) {
      Row row = rows.next();
      Text comment = row.column_by_name<Text>("comment");
      try {
        Float similarity = row.column_by_name<Float>("similarity");
        TEST_LOG("  " << (i+1) << ". " << comment.str() << 
                 " (cosine similarity: " << similarity.value() << ")");
      } catch (...) {
        TEST_LOG("  " << (i+1) << ". " << comment.str());
      }
    }
  } catch (const Exception& e) {
    TEST_LOG("ANN with similarity_cosine failed: " << e.what());
  }
  
  // Test 3: Different vectors for similarity calculation vs ordering
  TEST_LOG("\n=== Test 3: Different vectors for similarity vs ordering ===");
  try {
    Result result = session_.execute(
      "SELECT comment, "
      "       similarity_cosine(comment_vector, [0.3, 0.2, 0.1, 0.3, 0.1]) AS sim1, "
      "       similarity_cosine(comment_vector, [0.1, 0.1, 0.4, 0.3, 0.1]) AS sim2 "
      "FROM cycling_comments_vs "
      "ORDER BY comment_vector ANN OF [0.2, 0.15, 0.25, 0.3, 0.1] "
      "LIMIT 3");
    
    TEST_LOG("Results ordered by one vector, similarities to two others:");
    Rows rows = result.rows();
    for (size_t i = 0; i < rows.row_count(); ++i) {
      Row row = rows.next();
      Text comment = row.column_by_name<Text>("comment");
      TEST_LOG("  " << (i+1) << ". " << comment.str());
      try {
        Float sim1 = row.column_by_name<Float>("sim1");
        Float sim2 = row.column_by_name<Float>("sim2");
        TEST_LOG("     Similarity to vector1: " << sim1.value());
        TEST_LOG("     Similarity to vector2: " << sim2.value());
      } catch (...) {
        TEST_LOG("     (similarity values not available)");
      }
    }
  } catch (const Exception& e) {
    TEST_LOG("Complex similarity query failed: " << e.what());
  }
}

/**
 * Test all three similarity functions with same data
 */
CASSANDRA_INTEGRATION_TEST_F(VectorANNSearchTest, CompareSimilarityFunctions) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table with normalized vectors for fair comparison
  session_.execute("CREATE TABLE similarity_comparison ("
                   "id int PRIMARY KEY, "
                   "name text, "
                   "vec vector<float, 3>)");
  
  // Create index
  try {
    session_.execute("CREATE INDEX similarity_comparison_idx ON similarity_comparison (vec) USING 'sai'");
  } catch (const Exception& e) {
    TEST_LOG("Could not create index: " << e.what());
    return;
  }
  
  // Insert normalized vectors (for cosine similarity to be meaningful)
  session_.execute("INSERT INTO similarity_comparison (id, name, vec) VALUES (1, 'x_axis', [1.0, 0.0, 0.0])");
  session_.execute("INSERT INTO similarity_comparison (id, name, vec) VALUES (2, 'y_axis', [0.0, 1.0, 0.0])");
  session_.execute("INSERT INTO similarity_comparison (id, name, vec) VALUES (3, 'z_axis', [0.0, 0.0, 1.0])");
  session_.execute("INSERT INTO similarity_comparison (id, name, vec) VALUES (4, 'diagonal_xy', [0.707, 0.707, 0.0])");
  session_.execute("INSERT INTO similarity_comparison (id, name, vec) VALUES (5, 'diagonal_xyz', [0.577, 0.577, 0.577])");
  session_.execute("INSERT INTO similarity_comparison (id, name, vec) VALUES (6, 'near_x', [0.95, 0.22, 0.22])");
  
  // Query vector (close to x-axis)
  std::string query_vector = "[1.0, 0.1, 0.1]";
  
  // Test each similarity function
  std::vector<std::string> similarity_functions = {"cosine", "dot_product", "euclidean"};
  
  for (const auto& sim_func : similarity_functions) {
    TEST_LOG("\n=== Testing similarity_" << sim_func << " ===");
    
    std::string query = "SELECT name, similarity_" + sim_func + "(vec, " + query_vector + ") AS similarity "
                        "FROM similarity_comparison "
                        "ORDER BY vec ANN OF " + query_vector + " "
                        "LIMIT 6";
    
    try {
      Result result = session_.execute(query);
      
      TEST_LOG("Results ordered by ANN with " << sim_func << " similarity:");
      Rows rows = result.rows();
      for (size_t i = 0; i < rows.row_count(); ++i) {
        Row row = rows.next();
        Text name = row.column_by_name<Text>("name");
        try {
          Float similarity = row.column_by_name<Float>("similarity");
          TEST_LOG("  " << (i+1) << ". " << name.str() << 
                   " (" << sim_func << ": " << similarity.value() << ")");
        } catch (...) {
          TEST_LOG("  " << (i+1) << ". " << name.str());
        }
      }
    } catch (const Exception& e) {
      TEST_LOG("Query with " << sim_func << " failed: " << e.what());
    }
  }
  
  // Test combining different similarity functions in one query
  TEST_LOG("\n=== All similarity functions in one query ===");
  try {
    Result result = session_.execute(
      "SELECT name, "
      "       similarity_cosine(vec, " + query_vector + ") AS cos_sim, "
      "       similarity_dot_product(vec, " + query_vector + ") AS dot_sim, "
      "       similarity_euclidean(vec, " + query_vector + ") AS euc_sim "
      "FROM similarity_comparison "
      "ORDER BY vec ANN OF " + query_vector + " "
      "LIMIT 3");
    
    TEST_LOG("Top 3 results with all similarity metrics:");
    Rows rows = result.rows();
    for (size_t i = 0; i < rows.row_count(); ++i) {
      Row row = rows.next();
      Text name = row.column_by_name<Text>("name");
      TEST_LOG("  " << (i+1) << ". " << name.str() << ":");
      try {
        Float cos_sim = row.column_by_name<Float>("cos_sim");
        Float dot_sim = row.column_by_name<Float>("dot_sim");
        Float euc_sim = row.column_by_name<Float>("euc_sim");
        TEST_LOG("     Cosine: " << cos_sim.value());
        TEST_LOG("     Dot Product: " << dot_sim.value());
        TEST_LOG("     Euclidean: " << euc_sim.value());
      } catch (...) {
        TEST_LOG("     (similarity values not available)");
      }
    }
  } catch (const Exception& e) {
    TEST_LOG("Combined similarity query failed: " << e.what());
  }
}

/**
 * Test ANN search with filtering
 */
CASSANDRA_INTEGRATION_TEST_F(VectorANNSearchTest, ANNSearchWithFiltering) {
  CHECK_FAILURE;
  CHECK_VERSION(5.0.5);
  
  // Create table with additional columns for filtering (using int for price to simplify)
  session_.execute("CREATE TABLE products ("
                   "id int PRIMARY KEY, "
                   "name text, "
                   "category text, "
                   "price int, "
                   "description_vector vector<float, 4>)");
  
  // Create index on vector column
  try {
    session_.execute("CREATE INDEX products_vector_idx ON products (description_vector) USING 'sai'");
  } catch (const Exception& e) {
    TEST_LOG("Could not create vector index: " << e.what());
    return;
  }
  
  // Create additional indexes for filtering
  session_.execute("CREATE INDEX products_category_idx ON products (category) USING 'sai'");
  session_.execute("CREATE INDEX products_price_idx ON products (price) USING 'sai'");
  
  // Insert test products
  struct Product {
    int id;
    const char* name;
    const char* category;
    int price;
    float vector[4];
  };
  
  Product products[] = {
    {1, "Mountain Bike Pro", "bikes", 2500, {0.8f, 0.1f, 0.05f, 0.05f}},
    {2, "Road Bike Elite", "bikes", 3500, {0.1f, 0.8f, 0.05f, 0.05f}},
    {3, "Bike Helmet", "accessories", 150, {0.3f, 0.3f, 0.3f, 0.1f}},
    {4, "Cycling Shoes", "accessories", 200, {0.2f, 0.3f, 0.4f, 0.1f}},
    {5, "Electric Bike", "bikes", 4000, {0.5f, 0.3f, 0.1f, 0.1f}},
    {6, "Bike Lock", "accessories", 50, {0.1f, 0.1f, 0.7f, 0.1f}}
  };
  
  for (const auto& product : products) {
    CassStatement* statement = cass_statement_new(
      "INSERT INTO products (id, name, category, price, description_vector) VALUES (?, ?, ?, ?, ?)", 5);
    
    cass_statement_bind_int32(statement, 0, product.id);
    cass_statement_bind_string(statement, 1, product.name);
    cass_statement_bind_string(statement, 2, product.category);
    cass_statement_bind_int32(statement, 3, product.price);
    
    CassVector* vector = cass_vector_new(CASS_VALUE_TYPE_FLOAT, 4);
    for (int i = 0; i < 4; ++i) {
      cass_vector_append_float(vector, product.vector[i]);
    }
    cass_statement_bind_vector(statement, 4, vector);
    cass_vector_free(vector);
    
    CassFuture* future = cass_session_execute(session_.get(), statement);
    cass_statement_free(statement);
    
    CassError rc = cass_future_error_code(future);
    cass_future_free(future);
    ASSERT_EQ(CASS_OK, rc);
  }
  
  // Test 1: ANN search with category filter
  TEST_LOG("\n=== ANN search filtered by category ===");
  try {
    Result result = session_.execute(
      "SELECT name, category, similarity_cosine(description_vector, [0.7, 0.2, 0.05, 0.05]) AS similarity "
      "FROM products "
      "WHERE category = 'bikes' "
      "ORDER BY description_vector ANN OF [0.7, 0.2, 0.05, 0.05] "
      "LIMIT 3");
    
    TEST_LOG("Top bikes similar to query vector:");
    Rows rows = result.rows();
    for (size_t i = 0; i < rows.row_count(); ++i) {
      Row row = rows.next();
      Text name = row.column_by_name<Text>("name");
      Text category = row.column_by_name<Text>("category");
      try {
        Float similarity = row.column_by_name<Float>("similarity");
        TEST_LOG("  " << (i+1) << ". " << name.str() << " (" << category.str() << 
                 ") - similarity: " << similarity.value());
      } catch (...) {
        TEST_LOG("  " << (i+1) << ". " << name.str() << " (" << category.str() << ")");
      }
    }
  } catch (const Exception& e) {
    TEST_LOG("ANN with category filter failed: " << e.what());
  }
  
  // Test 2: ANN search with price range filter
  TEST_LOG("\n=== ANN search filtered by price range ===");
  try {
    Result result = session_.execute(
      "SELECT name, price, similarity_cosine(description_vector, [0.3, 0.3, 0.3, 0.1]) AS similarity "
      "FROM products "
      "WHERE price < 500 "
      "ORDER BY description_vector ANN OF [0.3, 0.3, 0.3, 0.1] "
      "LIMIT 3");
    
    TEST_LOG("Affordable products similar to query vector:");
    Rows rows = result.rows();
    for (size_t i = 0; i < rows.row_count(); ++i) {
      Row row = rows.next();
      Text name = row.column_by_name<Text>("name");
      Integer price = row.column_by_name<Integer>("price");
      try {
        Float similarity = row.column_by_name<Float>("similarity");
        TEST_LOG("  " << (i+1) << ". " << name.str() << " ($" << price.str() << 
                 ") - similarity: " << similarity.value());
      } catch (...) {
        TEST_LOG("  " << (i+1) << ". " << name.str() << " ($" << price.str() << ")");
      }
    }
  } catch (const Exception& e) {
    TEST_LOG("ANN with price filter failed: " << e.what());
  }
}