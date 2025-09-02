package main

import (
	"encoding/hex"
	"fmt"
	"math"
	"path/filepath"
	"runtime"
	
	// Use the actual gocql driver from reference-repos
	"github.com/gocql/gocql"
)

func init() {
	// Get the path to the reference Go driver
	_, filename, _, _ := runtime.Caller(0)
	dir := filepath.Dir(filename)
	fmt.Printf("Using Go driver from: %s\n", filepath.Join(dir, "../../../reference-repos/cassandra-gocql-driver"))
}

func main() {
	fmt.Println("=== Go Float Vector Encoding Validation (Using Real Driver) ===")
	fmt.Println()
	
	// Test vectors
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
	
	// Create vector type using actual gocql
	floatType := gocql.NewNativeType(5, gocql.TypeFloat, "")
	
	for _, tc := range testCases {
		// Create VectorType with float element type
		vecType := gocql.VectorType{
			SubType:    floatType,
			Dimensions: len(tc.values),
		}
		
		// Marshal the vector using the actual driver
		encoded, err := vecType.Marshal(tc.values)
		if err != nil {
			fmt.Printf("Error marshaling %s: %v\n", tc.name, err)
			continue
		}
		
		fmt.Printf("Test: %s\n", tc.name)
		fmt.Printf("Values: %v\n", tc.values)
		fmt.Printf("Dimension: %d\n", len(tc.values))
		fmt.Printf("Encoded size: %d bytes\n", len(encoded))
		fmt.Printf("Hex: %s\n", hex.EncodeToString(encoded))
		fmt.Println()
	}
	
	// Special output format for validation
	fmt.Println("=== VALIDATION DATA ===")
	fmt.Println("For C++ comparison, key test vectors:")
	fmt.Println()
	
	// Standard test vector
	vecType3 := gocql.VectorType{
		SubType:    floatType,
		Dimensions: 3,
	}
	standard := []float32{1.0, 2.0, 3.0}
	standardBytes, _ := vecType3.Marshal(standard)
	fmt.Printf("vector<float, 3> = [1.0, 2.0, 3.0]\n")
	fmt.Printf("GO_BYTES: %s\n", hex.EncodeToString(standardBytes))
	fmt.Println()
	
	// Edge cases
	edges := []float32{0.0, -1.0, float32(math.Inf(1))}
	edgeBytes, _ := vecType3.Marshal(edges)
	fmt.Printf("vector<float, 3> = [0.0, -1.0, +Inf]\n")
	fmt.Printf("GO_BYTES: %s\n", hex.EncodeToString(edgeBytes))
	fmt.Println()
	
	// Large vector
	vecType100 := gocql.VectorType{
		SubType:    floatType,
		Dimensions: 100,
	}
	large := make([]float32, 100)
	for i := range large {
		large[i] = float32(i)
	}
	largeBytes, _ := vecType100.Marshal(large)
	fmt.Printf("vector<float, 100> = [0.0, 1.0, 2.0, ..., 99.0]\n")
	fmt.Printf("GO_BYTES size: %d\n", len(largeBytes))
	fmt.Printf("GO_BYTES first 32 bytes: %s\n", hex.EncodeToString(largeBytes[:32]))
}