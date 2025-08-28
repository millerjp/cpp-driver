#include <gtest/gtest.h>
#include "vector_value.hpp"
#include "data_type.hpp"
#include "buffer.hpp"

using namespace datastax::internal::core;

TEST(VectorEncodingTest, SmallintVectorHasLengthPrefixes) {
  // Create vector type for smallint
  DataType::ConstPtr smallint_type(new DataType(CASS_VALUE_TYPE_SMALL_INT));
  VectorType::ConstPtr vector_type(new VectorType(smallint_type, 3));
  
  // Create vector value
  VectorValue vector((DataType::ConstPtr(vector_type)));
  
  // Add three smallint values
  vector.append_int16(-32768);
  vector.append_int16(0);
  vector.append_int16(32767);
  
  // Get encoded buffer
  Buffer encoded = vector.encode();
  
  // Should be 12 bytes: 3 * (2 byte length prefix + 2 byte value)
  ASSERT_EQ(12u, encoded.size());
  
  const char* data = encoded.data();
  
  // Check length prefixes (00 02 for 2-byte values)
  EXPECT_EQ(0x00, data[0]);
  EXPECT_EQ(0x02, data[1]);
  
  EXPECT_EQ(0x00, data[4]);
  EXPECT_EQ(0x02, data[5]);
  
  EXPECT_EQ(0x00, data[8]);
  EXPECT_EQ(0x02, data[9]);
  
  // Check actual values
  EXPECT_EQ((char)0x80, data[2]); // -32768 high byte
  EXPECT_EQ((char)0x00, data[3]); // -32768 low byte
  
  EXPECT_EQ((char)0x00, data[6]); // 0 high byte
  EXPECT_EQ((char)0x00, data[7]); // 0 low byte
  
  EXPECT_EQ((char)0x7f, data[10]); // 32767 high byte
  EXPECT_EQ((char)0xff, data[11]); // 32767 low byte
}

TEST(VectorEncodingTest, TinyintVectorHasLengthPrefixes) {
  // Create vector type for tinyint
  DataType::ConstPtr tinyint_type(new DataType(CASS_VALUE_TYPE_TINY_INT));
  VectorType::ConstPtr vector_type(new VectorType(tinyint_type, 3));
  
  // Create vector value
  VectorValue vector((DataType::ConstPtr(vector_type)));
  
  // Add three tinyint values
  vector.append_int8(-128);
  vector.append_int8(0);
  vector.append_int8(127);
  
  // Get encoded buffer
  Buffer encoded = vector.encode();
  
  // Should be 9 bytes: 3 * (2 byte length prefix + 1 byte value)
  ASSERT_EQ(9u, encoded.size());
  
  const char* data = encoded.data();
  
  // Check length prefixes (00 01 for 1-byte values)
  EXPECT_EQ(0x00, data[0]);
  EXPECT_EQ(0x01, data[1]);
  
  EXPECT_EQ(0x00, data[3]);
  EXPECT_EQ(0x01, data[4]);
  
  EXPECT_EQ(0x00, data[6]);
  EXPECT_EQ(0x01, data[7]);
  
  // Check actual values
  EXPECT_EQ((char)0x80, data[2]); // -128
  EXPECT_EQ((char)0x00, data[5]); // 0
  EXPECT_EQ((char)0x7f, data[8]); // 127
}

TEST(VectorEncodingTest, IntVectorHasNoLengthPrefixes) {
  // Create vector type for int
  DataType::ConstPtr int_type(new DataType(CASS_VALUE_TYPE_INT));
  VectorType::ConstPtr vector_type(new VectorType(int_type, 3));
  
  // Create vector value
  VectorValue vector((DataType::ConstPtr(vector_type)));
  
  // Add three int values
  vector.append_int32(-2147483648);
  vector.append_int32(0);
  vector.append_int32(2147483647);
  
  // Get encoded buffer
  Buffer encoded = vector.encode();
  
  // Should be 12 bytes: 3 * 4 byte value (NO length prefixes)
  ASSERT_EQ(12u, encoded.size());
  
  const char* data = encoded.data();
  
  // First int should start immediately (no length prefix)
  EXPECT_EQ((char)0x80, data[0]);
  EXPECT_EQ((char)0x00, data[1]);
  EXPECT_EQ((char)0x00, data[2]);
  EXPECT_EQ((char)0x00, data[3]);
  
  // Second int at offset 4
  EXPECT_EQ((char)0x00, data[4]);
  EXPECT_EQ((char)0x00, data[5]);
  EXPECT_EQ((char)0x00, data[6]);
  EXPECT_EQ((char)0x00, data[7]);
  
  // Third int at offset 8
  EXPECT_EQ((char)0x7f, data[8]);
  EXPECT_EQ((char)0xff, data[9]);
  EXPECT_EQ((char)0xff, data[10]);
  EXPECT_EQ((char)0xff, data[11]);
}

TEST(VectorEncodingTest, FloatVectorHasNoLengthPrefixes) {
  // Create vector type for float
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  VectorType::ConstPtr vector_type(new VectorType(float_type, 3));
  
  // Create vector value
  VectorValue vector((DataType::ConstPtr(vector_type)));
  
  // Add three float values
  vector.append_float(1.0f);
  vector.append_float(-2.5f);
  vector.append_float(3.14159f);
  
  // Get encoded buffer
  Buffer encoded = vector.encode();
  
  // Should be 12 bytes: 3 * 4 byte value (NO length prefixes)
  ASSERT_EQ(12u, encoded.size());
  
  // Just verify no length prefix pattern (00 04 would be wrong)
  const char* data = encoded.data();
  // The first byte should be part of the float encoding, not a length prefix
  EXPECT_NE(0x00, data[0] | data[1]); // At least one should be non-zero for 1.0f
}