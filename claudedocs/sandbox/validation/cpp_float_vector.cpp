/*
  C++ Float Vector Encoding Validation
*/

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cmath>

// Include the actual implementation files
#include "../../../src/uvint.cpp"
#include "../../../src/vector_type.cpp"
#include "../../../src/cass_vector.cpp"
#include "../../../src/encode.hpp"
#include "../../../src/buffer.hpp"
#include "../../../src/serialization.hpp"

using namespace datastax::internal::core;

// Helper to print bytes as hex
void print_hex(const char* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') 
              << (static_cast<unsigned int>(static_cast<unsigned char>(data[i])));
  }
}

// Test encoding a vector of floats
void test_float_vector(const std::string& name, const std::vector<float>& values) {
  // Create vector type and instance
  DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
  CassandraVector vec(float_type, values.size());
  
  // Add all values
  for (float value : values) {
    CassError err = vec.append(value);
    if (err != CASS_OK) {
      std::cout << "Error appending value: " << value << std::endl;
      return;
    }
  }
  
  // Encode the vector
  Buffer encoded = vec.encode();
  
  // Print results
  std::cout << "Test: " << name << std::endl;
  std::cout << "Values:";
  for (float v : values) {
    std::cout << " " << v;
  }
  std::cout << std::endl;
  std::cout << "Dimension: " << values.size() << std::endl;
  std::cout << "Encoded size: " << encoded.size() << " bytes" << std::endl;
  std::cout << "Hex: ";
  print_hex(encoded.data(), encoded.size());
  std::cout << std::endl;
  
  // Show breakdown
  std::cout << "Breakdown:" << std::endl;
  for (size_t i = 0; i < values.size(); i++) {
    Buffer single_float = encode(values[i]);
    std::cout << "  [" << i << "] " << std::fixed << std::setprecision(6) 
              << values[i] << " -> ";
    print_hex(single_float.data(), single_float.size());
    std::cout << std::endl;
  }
  
  std::cout << std::endl;
}

int main() {
  std::cout << "=== C++ Float Vector Encoding Validation ===" << std::endl;
  std::cout << std::endl;
  
  // Test cases matching Go program
  test_float_vector("Simple [1.0, 2.0, 3.0]", {1.0f, 2.0f, 3.0f});
  test_float_vector("Negative [-1.5, 0.0, 1.5]", {-1.5f, 0.0f, 1.5f});
  test_float_vector("Large [1234.5678, -9876.5432]", {1234.5678f, -9876.5432f});
  test_float_vector("Single [3.14159]", {3.14159f});
  
  // Special values
  float neg_zero = -0.0f;
  float pos_inf = std::numeric_limits<float>::infinity();
  float neg_inf = -std::numeric_limits<float>::infinity();
  float nan_val = std::numeric_limits<float>::quiet_NaN();
  test_float_vector("Special [0.0, -0.0, Inf, -Inf, NaN]", 
                   {0.0f, neg_zero, pos_inf, neg_inf, nan_val});
  
  // Ten elements
  test_float_vector("Ten elements", 
                   {1.1f, 2.2f, 3.3f, 4.4f, 5.5f, 6.6f, 7.7f, 8.8f, 9.9f, 10.10f});
  
  // Max/Min
  test_float_vector("Max/Min normal", 
                   {std::numeric_limits<float>::max(), 
                    -std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::min()});
  
  // Validation data for comparison
  std::cout << "=== VALIDATION DATA ===" << std::endl;
  std::cout << "For Go comparison, key test vectors:" << std::endl;
  std::cout << std::endl;
  
  // Standard test vector
  {
    DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
    CassandraVector vec(float_type, 3);
    vec.append(1.0f);
    vec.append(2.0f);
    vec.append(3.0f);
    Buffer encoded = vec.encode();
    
    std::cout << "vector<float, 3> = [1.0, 2.0, 3.0]" << std::endl;
    std::cout << "CPP_BYTES: ";
    print_hex(encoded.data(), encoded.size());
    std::cout << std::endl;
    std::cout << "Expected: 3f800000 40000000 40400000 (12 bytes, no prefixes)" << std::endl;
    std::cout << std::endl;
  }
  
  // Edge cases
  {
    DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
    CassandraVector vec(float_type, 3);
    vec.append(0.0f);
    vec.append(-1.0f);
    vec.append(std::numeric_limits<float>::infinity());
    Buffer encoded = vec.encode();
    
    std::cout << "vector<float, 3> = [0.0, -1.0, +Inf]" << std::endl;
    std::cout << "CPP_BYTES: ";
    print_hex(encoded.data(), encoded.size());
    std::cout << std::endl;
    std::cout << std::endl;
  }
  
  // Large vector
  {
    DataType::ConstPtr float_type(new DataType(CASS_VALUE_TYPE_FLOAT));
    CassandraVector vec(float_type, 100);
    for (int i = 0; i < 100; i++) {
      vec.append(static_cast<float>(i));
    }
    Buffer encoded = vec.encode();
    
    std::cout << "vector<float, 100> = [0.0, 1.0, 2.0, ..., 99.0]" << std::endl;
    std::cout << "CPP_BYTES size: " << encoded.size() << std::endl;
    std::cout << "CPP_BYTES first 32 bytes: ";
    print_hex(encoded.data(), 32);
    std::cout << std::endl;
  }
  
  return 0;
}