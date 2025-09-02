package main

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"math"
)

// VectorType represents a Cassandra vector type
type VectorType struct {
	SubType    string
	Dimensions int
}

// isFixedLength returns true for types that don't need UVINT prefix
func isFixedLength(subType string) bool {
	switch subType {
	case "float", "double", "int", "bigint", "boolean", "uuid", "timeuuid", "timestamp":
		return true
	default:
		return false
	}
}

// encodeFloat encodes a float32 to bytes (big-endian)
func encodeFloat(value float32) []byte {
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.BigEndian, value)
	return buf.Bytes()
}

// encodeFloatVector encodes a vector of floats
func encodeFloatVector(values []float32) []byte {
	var result bytes.Buffer
	
	// For fixed-length types like float, no UVINT prefix needed
	for _, value := range values {
		result.Write(encodeFloat(value))
	}
	
	return result.Bytes()
}

func main() {
	fmt.Println("=== Go Float Vector Encoding Validation ===")
	fmt.Println()
	
	// Test cases with various float vectors
	testCases := []struct {
		name   string
		values []float32
	}{
		{
			name:   "Simple [1.0, 2.0, 3.0]",
			values: []float32{1.0, 2.0, 3.0},
		},
		{
			name:   "Negative [-1.5, 0.0, 1.5]",
			values: []float32{-1.5, 0.0, 1.5},
		},
		{
			name:   "Large [1234.5678, -9876.5432]",
			values: []float32{1234.5678, -9876.5432},
		},
		{
			name:   "Single [3.14159]",
			values: []float32{3.14159},
		},
		{
			name:   "Special [0.0, -0.0, Inf, -Inf, NaN]",
			values: []float32{0.0, float32(math.Copysign(0, -1)), 
			                  float32(math.Inf(1)), float32(math.Inf(-1)), 
			                  float32(math.NaN())},
		},
		{
			name:   "Ten elements",
			values: []float32{1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.10},
		},
		{
			name:   "Max/Min normal",
			values: []float32{math.MaxFloat32, -math.MaxFloat32, math.SmallestNonzeroFloat32},
		},
	}
	
	for _, tc := range testCases {
		encoded := encodeFloatVector(tc.values)
		
		fmt.Printf("Test: %s\n", tc.name)
		fmt.Printf("Values: %v\n", tc.values)
		fmt.Printf("Dimension: %d\n", len(tc.values))
		fmt.Printf("Encoded size: %d bytes\n", len(encoded))
		fmt.Printf("Hex: %s\n", hex.EncodeToString(encoded))
		
		// Also show individual float encodings for clarity
		fmt.Println("Breakdown:")
		for i, value := range tc.values {
			floatBytes := encodeFloat(value)
			fmt.Printf("  [%d] %.6f -> %s\n", i, value, hex.EncodeToString(floatBytes))
		}
		
		fmt.Println()
	}
	
	// Special output format for easy comparison
	fmt.Println("=== VALIDATION DATA ===")
	fmt.Println("For C++ comparison, key test vectors:")
	fmt.Println()
	
	// Standard test vector
	standard := []float32{1.0, 2.0, 3.0}
	standardBytes := encodeFloatVector(standard)
	fmt.Printf("vector<float, 3> = [1.0, 2.0, 3.0]\n")
	fmt.Printf("GO_BYTES: %s\n", hex.EncodeToString(standardBytes))
	fmt.Printf("Expected: 3f800000 40000000 40400000 (12 bytes, no prefixes)\n")
	fmt.Println()
	
	// Edge cases
	edges := []float32{0.0, -1.0, float32(math.Inf(1))}
	edgeBytes := encodeFloatVector(edges)
	fmt.Printf("vector<float, 3> = [0.0, -1.0, +Inf]\n")
	fmt.Printf("GO_BYTES: %s\n", hex.EncodeToString(edgeBytes))
	fmt.Println()
	
	// Large vector
	large := make([]float32, 100)
	for i := range large {
		large[i] = float32(i)
	}
	largeBytes := encodeFloatVector(large)
	fmt.Printf("vector<float, 100> = [0.0, 1.0, 2.0, ..., 99.0]\n")
	fmt.Printf("GO_BYTES size: %d\n", len(largeBytes))
	fmt.Printf("GO_BYTES first 32 bytes: %s\n", hex.EncodeToString(largeBytes[:32]))
}