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
#include "cass_vector.hpp"
#include "user_type_value.hpp"
#include "data_type.hpp"
#include "encode.hpp"
#include "buffer.hpp"

using namespace datastax::internal::core;

/**
 * Test vectors containing User-Defined Types (UDTs)
 */
class VectorUDTTest : public ::testing::Test {
protected:
  void SetUp() {
    // Create a user type definition
    UserType* user_type_def = new UserType("test_keyspace", "address", true);
    user_type_def->add_field("street", DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TEXT)));
    user_type_def->add_field("city", DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TEXT)));
    user_type_def->add_field("zip", DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_INT)));
    user_type_ = DataType::ConstPtr(user_type_def);
  }
  
  DataType::ConstPtr user_type_;
};

/**
 * Test creating a vector with UDT element type
 */
TEST_F(VectorUDTTest, CreateVectorWithUDTType) {
  // Create a vector type with UDT elements
  VectorType::ConstPtr vector_type(new VectorType(user_type_, 3));
  
  EXPECT_EQ(vector_type->dimension(), 3ul);
  EXPECT_EQ(vector_type->element_type(), user_type_);
  EXPECT_FALSE(vector_type->is_fixed_length_element());  // UDTs are variable-length
  
  // Create a vector instance
  CassandraVector vector(vector_type);
  EXPECT_EQ(vector.dimension(), 3ul);
  EXPECT_EQ(vector.size(), 0ul);
  EXPECT_FALSE(vector.is_full());
}

/**
 * Test appending UDT values to a vector
 */
TEST_F(VectorUDTTest, AppendUDTValues) {
  VectorType::ConstPtr vector_type(new VectorType(user_type_, 2));
  CassandraVector* vector = new CassandraVector(vector_type);
  vector->inc_ref();
  
  // Create first UDT value
  UserTypeValue udt1(user_type_);
  udt1.set(0, CassString("123 Main St", 11));
  udt1.set(1, CassString("New York", 8));
  udt1.set(2, cass_int32_t(10001));
  
  // Create second UDT value
  UserTypeValue udt2(user_type_);
  udt2.set(0, CassString("456 Oak Ave", 11));
  udt2.set(1, CassString("Los Angeles", 11));
  udt2.set(2, cass_int32_t(90001));
  
  // Append UDT values
  EXPECT_EQ(vector->append(&udt1), CASS_OK);
  EXPECT_EQ(vector->append(&udt2), CASS_OK);
  EXPECT_EQ(vector->size(), 2ul);
  EXPECT_TRUE(vector->is_full());
  
  // Try to append beyond dimension - should fail
  EXPECT_EQ(vector->append(&udt1), CASS_ERROR_LIB_INDEX_OUT_OF_BOUNDS);
  
  vector->dec_ref();
}

/**
 * Test C API for UDT vectors
 */
TEST_F(VectorUDTTest, CAPIWithUDTs) {
  // Create vector from data type
  CustomType* custom_type = new CustomType("org.apache.cassandra.db.marshal.VectorType("
                                           "org.apache.cassandra.db.marshal.UserType("
                                           "test_keyspace,address,"
                                           "737472656574:org.apache.cassandra.db.marshal.UTF8Type,"
                                           "63697479:org.apache.cassandra.db.marshal.UTF8Type,"
                                           "7a6970:org.apache.cassandra.db.marshal.Int32Type), 2)");
  CassDataType* cass_data_type = CassDataType::to(custom_type);
  
  CassVector* vector = cass_vector_new_from_data_type(cass_data_type);
  // Note: In real usage, this would need proper type parsing
  // For unit test, we'll use the simpler constructor
  cass_vector_free(vector);
  
  // Create vector with manual type
  VectorType::ConstPtr vector_type(new VectorType(user_type_, 2));
  CassandraVector* cpp_vector = new CassandraVector(vector_type);
  cpp_vector->inc_ref();
  vector = CassVector::to(cpp_vector);
  
  // Create UDT values through C API
  CassUserType* udt1 = cass_user_type_new_from_data_type(CassDataType::to(user_type_.get()));
  cass_user_type_set_string_by_name(udt1, "street", "789 Elm St");
  cass_user_type_set_string_by_name(udt1, "city", "Chicago");
  cass_user_type_set_int32_by_name(udt1, "zip", 60601);
  
  CassUserType* udt2 = cass_user_type_new_from_data_type(CassDataType::to(user_type_.get()));
  cass_user_type_set_string_by_name(udt2, "street", "321 Pine Rd");
  cass_user_type_set_string_by_name(udt2, "city", "Seattle");
  cass_user_type_set_int32_by_name(udt2, "zip", 98101);
  
  // Append to vector
  EXPECT_EQ(cass_vector_append_user_type(vector, udt1), CASS_OK);
  EXPECT_EQ(cass_vector_append_user_type(vector, udt2), CASS_OK);
  
  // Check dimension
  EXPECT_EQ(cass_vector_dimension(vector), 2ul);
  
  // Clean up
  cass_user_type_free(udt1);
  cass_user_type_free(udt2);
  cass_vector_free(vector);
  delete custom_type;
}

/**
 * Test encoding of UDT vectors
 */
TEST_F(VectorUDTTest, EncodingUDTVector) {
  VectorType::ConstPtr vector_type(new VectorType(user_type_, 2));
  CassandraVector vector(vector_type);
  
  // Create and append UDT values
  UserTypeValue udt1(user_type_);
  udt1.set(0, CassString("Test St", 7));
  udt1.set(1, CassString("Boston", 6));
  udt1.set(2, cass_int32_t(2101));
  
  UserTypeValue udt2(user_type_);
  udt2.set(0, CassString("Demo Ave", 8));
  udt2.set(1, CassString("Miami", 5));
  udt2.set(2, cass_int32_t(33101));
  
  ASSERT_EQ(vector.append(&udt1), CASS_OK);
  ASSERT_EQ(vector.append(&udt2), CASS_OK);
  
  // Encode the vector
  Buffer encoded = vector.encode();
  
  // Verify encoding contains data (UDTs are variable-length, so each has UVINT prefix)
  EXPECT_GT(encoded.size(), 0ul);
  
  // The encoded buffer should contain:
  // - UVINT size + UDT1 data
  // - UVINT size + UDT2 data
  
  // Get individual UDT encodings to verify
  Buffer udt1_encoded = udt1.encode();
  Buffer udt2_encoded = udt2.encode();
  
  // Total size should be UVINT sizes + UDT data
  size_t expected_size = uvint_size(udt1_encoded.size()) + udt1_encoded.size() +
                        uvint_size(udt2_encoded.size()) + udt2_encoded.size();
  EXPECT_EQ(encoded.size(), expected_size);
}

/**
 * Test nested UDT in vector of vectors
 */
TEST_F(VectorUDTTest, NestedUDTVector) {
  // Create inner vector type: vector<UDT, 2>
  VectorType::ConstPtr inner_vector_type(new VectorType(user_type_, 2));
  
  // Create outer vector type: vector<frozen<vector<UDT, 2>>, 2>
  VectorType::ConstPtr outer_vector_type(new VectorType(
    DataType::ConstPtr(inner_vector_type), 2));
  
  CassandraVector* outer = new CassandraVector(outer_vector_type);
  outer->inc_ref();
  
  // Create first inner vector with UDTs
  CassandraVector* inner1 = new CassandraVector(inner_vector_type);
  inner1->inc_ref();
  
  UserTypeValue udt1(user_type_);
  udt1.set(0, CassString("Street 1", 8));
  udt1.set(1, CassString("City 1", 6));
  udt1.set(2, cass_int32_t(11111));
  
  UserTypeValue udt2(user_type_);
  udt2.set(0, CassString("Street 2", 8));
  udt2.set(1, CassString("City 2", 6));
  udt2.set(2, cass_int32_t(22222));
  
  EXPECT_EQ(inner1->append(&udt1), CASS_OK);
  EXPECT_EQ(inner1->append(&udt2), CASS_OK);
  
  // Create second inner vector with UDTs
  CassandraVector* inner2 = new CassandraVector(inner_vector_type);
  inner2->inc_ref();
  
  UserTypeValue udt3(user_type_);
  udt3.set(0, CassString("Street 3", 8));
  udt3.set(1, CassString("City 3", 6));
  udt3.set(2, cass_int32_t(33333));
  
  UserTypeValue udt4(user_type_);
  udt4.set(0, CassString("Street 4", 8));
  udt4.set(1, CassString("City 4", 6));
  udt4.set(2, cass_int32_t(44444));
  
  EXPECT_EQ(inner2->append(&udt3), CASS_OK);
  EXPECT_EQ(inner2->append(&udt4), CASS_OK);
  
  // Append inner vectors to outer
  EXPECT_EQ(outer->append(inner1), CASS_OK);
  EXPECT_EQ(outer->append(inner2), CASS_OK);
  
  EXPECT_TRUE(outer->is_full());
  
  // Clean up
  inner1->dec_ref();
  inner2->dec_ref();
  outer->dec_ref();
}

/**
 * Test type validation for UDT vectors
 */
TEST_F(VectorUDTTest, TypeValidation) {
  // Create a different UDT type
  UserType* other_type_def = new UserType("test_keyspace", "person", true);
  other_type_def->add_field("name", DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_TEXT)));
  other_type_def->add_field("age", DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_INT)));
  DataType::ConstPtr other_type(other_type_def);
  
  // Create vector expecting address UDT
  VectorType::ConstPtr vector_type(new VectorType(user_type_, 2));
  CassandraVector vector(vector_type);
  
  // Create address UDT (correct type)
  UserTypeValue correct_udt(user_type_);
  correct_udt.set(0, CassString("Valid St", 8));
  correct_udt.set(1, CassString("Valid City", 10));
  correct_udt.set(2, cass_int32_t(12345));
  
  // Create person UDT (wrong type)
  UserTypeValue wrong_udt(other_type);
  wrong_udt.set(0, CassString("John Doe", 8));
  wrong_udt.set(1, cass_int32_t(30));
  
  // Correct type should succeed
  EXPECT_EQ(vector.append(&correct_udt), CASS_OK);
  
  // Wrong type should fail
  EXPECT_EQ(vector.append(&wrong_udt), CASS_ERROR_LIB_INVALID_VALUE_TYPE);
}

/**
 * Test UDT with null fields in vector
 */
TEST_F(VectorUDTTest, UDTWithNullFields) {
  VectorType::ConstPtr vector_type(new VectorType(user_type_, 2));
  CassandraVector vector(vector_type);
  
  // Create UDT with some null fields
  UserTypeValue udt1(user_type_);
  udt1.set(0, CassString("Only Street", 11));
  // Leave city as null (index 1)
  udt1.set(2, cass_int32_t(99999));
  
  UserTypeValue udt2(user_type_);
  // Leave street as null (index 0)
  udt2.set(1, CassString("Only City", 9));
  udt2.set(2, cass_int32_t(88888));
  
  // Both should append successfully - null fields in UDTs are valid
  EXPECT_EQ(vector.append(&udt1), CASS_OK);
  EXPECT_EQ(vector.append(&udt2), CASS_OK);
  
  // Encode and verify it doesn't crash
  Buffer encoded = vector.encode();
  EXPECT_GT(encoded.size(), 0ul);
}