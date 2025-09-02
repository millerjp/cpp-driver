/*
  C++ Float Vector Encoding Test - Standalone
*/

#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cmath>
#include <limits>
#include <cstdint>

// Simplified encode_float function matching the driver
uint32_t float_to_bits(float value) {
  uint32_t result;
  memcpy(&result, &value, sizeof(float));
  return result;
}

void encode_float_bigendian(uint8_t* output, float value) {
  uint32_t bits = float_to_bits(value);
  // Write in big-endian order
  output[0] = (bits >> 24) & 0xFF;
  output[1] = (bits >> 16) & 0xFF;
  output[2] = (bits >> 8) & 0xFF;
  output[3] = bits & 0xFF;
}

// Encode a float vector (no UVINT prefix for fixed-length types)
std::vector<uint8_t> encode_float_vector(const std::vector<float>& values) {
  std::vector<uint8_t> result(values.size() * 4);
  
  for (size_t i = 0; i < values.size(); i++) {
    encode_float_bigendian(&result[i * 4], values[i]);
  }
  
  return result;
}

void print_hex(const std::vector<uint8_t>& data) {
  for (uint8_t byte : data) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') 
              << static_cast<int>(byte);
  }
}

void test_vector(const std::string& name, const std::vector<float>& values) {
  auto encoded = encode_float_vector(values);
  
  std::cout << name << std::endl;
  std::cout << "Encoded bytes (" << encoded.size() << "): ";
  print_hex(encoded);
  std::cout << std::endl;
}

int main() {
  std::cout << "=== C++ Float Vector Encoding Test ===" << std::endl;
  std::cout << std::endl;
  
  // Test 1: Standard vector [1.0, 2.0, 3.0]
  test_vector("Test 1: vector<float, 3> = [1.0, 2.0, 3.0]", {1.0f, 2.0f, 3.0f});
  std::cout << "Expected:           3f8000004000000040400000" << std::endl;
  std::cout << std::endl;
  
  // Test 2: Edge cases [0.0, -1.0, +Inf]
  test_vector("Test 2: vector<float, 3> = [0.0, -1.0, +Inf]", 
              {0.0f, -1.0f, std::numeric_limits<float>::infinity()});
  std::cout << std::endl;
  
  // Test 3: Single element [3.14159]
  test_vector("Test 3: vector<float, 1> = [3.14159]", {3.14159f});
  std::cout << std::endl;
  
  // Test 4: Large vector first few elements
  std::vector<float> large(100);
  for (int i = 0; i < 100; i++) {
    large[i] = static_cast<float>(i);
  }
  auto large_encoded = encode_float_vector(large);
  std::cout << "Test 4: vector<float, 100> = [0.0, 1.0, 2.0, ..., 99.0]" << std::endl;
  std::cout << "Total size: " << large_encoded.size() << " bytes" << std::endl;
  std::cout << "First 32 bytes: ";
  for (int i = 0; i < 32; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') 
              << static_cast<int>(large_encoded[i]);
  }
  std::cout << std::endl << std::endl;
  
  // Test 5: Special values
  test_vector("Test 5: vector<float, 3> = [MaxFloat32, -MaxFloat32, SmallestNonzero]",
              {std::numeric_limits<float>::max(), 
               -std::numeric_limits<float>::max(),
               std::numeric_limits<float>::min()});
  std::cout << std::endl;
  
  // Validation reference
  std::cout << "=== VALIDATION REFERENCE ===" << std::endl;
  
  auto vec1 = encode_float_vector({1.0f, 2.0f, 3.0f});
  std::cout << "Standard [1.0, 2.0, 3.0]:     ";
  print_hex(vec1);
  std::cout << std::endl;
  
  auto vec2 = encode_float_vector({0.0f, -1.0f, std::numeric_limits<float>::infinity()});
  std::cout << "Edge [0.0, -1.0, +Inf]:       ";
  print_hex(vec2);
  std::cout << std::endl;
  
  auto vec3 = encode_float_vector({3.14159f});
  std::cout << "Single [3.14159]:             ";
  print_hex(vec3);
  std::cout << std::endl;
  
  std::cout << "Large first 32:               ";
  for (int i = 0; i < 32; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') 
              << static_cast<int>(large_encoded[i]);
  }
  std::cout << std::endl;
  
  return 0;
}