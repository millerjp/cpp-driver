package main

import (
	"bytes"
	"encoding/hex"
	"fmt"
	"math"
)

// Import the actual vector.go implementation
// We'll copy the key functions here since we can't import the local module directly

// From the Go driver vector.go:

// writeUnsignedVInt writes an unsigned variable integer
func writeUnsignedVInt(buf *bytes.Buffer, v uint64) {
	// Simplified - matches the Go driver implementation
	numBytes := computeUnsignedVIntSize(v)
	if numBytes <= 1 {
		buf.WriteByte(byte(v))
		return
	}

	extraBytes := numBytes - 1
	var tmp = make([]byte, numBytes)
	for i := extraBytes; i >= 0; i-- {
		tmp[i] = byte(v)
		v >>= 8
	}
	tmp[0] |= byte(^(0xff >> uint(extraBytes)))
	buf.Write(tmp)
}

// computeUnsignedVIntSize computes the size of an unsigned vint
func computeUnsignedVIntSize(v uint64) int {
	lead0 := 0
	if v == 0 {
		lead0 = 64
	} else {
		// Count leading zeros
		mask := uint64(1) << 63
		for v&mask == 0 {
			lead0++
			mask >>= 1
		}
	}
	return (639 - lead0*9) >> 6
}

// isVectorVariableLengthType checks if a type needs UVINT prefix
func isVectorVariableLengthType(typeName string) bool {
	switch typeName {
	case "bigint", "boolean", "timestamp", "double", "float", "int", "timeuuid", "uuid":
		return false
	default:
		return true
	}
}

// marshalFloatVector marshals a float vector
func marshalFloatVector(values []float32) []byte {
	buf := &bytes.Buffer{}
	
	// Float is fixed-length, so no UVINT prefix needed
	for _, value := range values {
		// Convert float to uint32 bits (IEEE 754)
		bits := math.Float32bits(value)
		// Write in big-endian order
		buf.WriteByte(byte(bits >> 24))
		buf.WriteByte(byte(bits >> 16))
		buf.WriteByte(byte(bits >> 8))
		buf.WriteByte(byte(bits))
	}
	
	return buf.Bytes()
}

func main() {
	fmt.Println("=== Go Driver Vector Encoding Test ===")
	fmt.Println()
	
	// Test 1: Standard vector [1.0, 2.0, 3.0]
	vec1 := []float32{1.0, 2.0, 3.0}
	encoded1 := marshalFloatVector(vec1)
	fmt.Printf("Test 1: vector<float, 3> = [1.0, 2.0, 3.0]\n")
	fmt.Printf("Encoded bytes (%d): %s\n", len(encoded1), hex.EncodeToString(encoded1))
	fmt.Printf("Expected:           3f80000040000000404000000\n")
	fmt.Println()
	
	// Test 2: Edge cases [0.0, -1.0, +Inf]
	vec2 := []float32{0.0, -1.0, float32(math.Inf(1))}
	encoded2 := marshalFloatVector(vec2)
	fmt.Printf("Test 2: vector<float, 3> = [0.0, -1.0, +Inf]\n")
	fmt.Printf("Encoded bytes (%d): %s\n", len(encoded2), hex.EncodeToString(encoded2))
	fmt.Println()
	
	// Test 3: Single element [3.14159]
	vec3 := []float32{3.14159}
	encoded3 := marshalFloatVector(vec3)
	fmt.Printf("Test 3: vector<float, 1> = [3.14159]\n")
	fmt.Printf("Encoded bytes (%d): %s\n", len(encoded3), hex.EncodeToString(encoded3))
	fmt.Println()
	
	// Test 4: Large vector first few elements
	vec4 := make([]float32, 100)
	for i := range vec4 {
		vec4[i] = float32(i)
	}
	encoded4 := marshalFloatVector(vec4)
	fmt.Printf("Test 4: vector<float, 100> = [0.0, 1.0, 2.0, ..., 99.0]\n")
	fmt.Printf("Total size: %d bytes\n", len(encoded4))
	fmt.Printf("First 32 bytes: %s\n", hex.EncodeToString(encoded4[:32]))
	fmt.Println()
	
	// Test 5: Special values
	vec5 := []float32{
		float32(math.MaxFloat32),
		float32(-math.MaxFloat32),
		float32(math.SmallestNonzeroFloat32),
	}
	encoded5 := marshalFloatVector(vec5)
	fmt.Printf("Test 5: vector<float, 3> = [MaxFloat32, -MaxFloat32, SmallestNonzero]\n")
	fmt.Printf("Encoded bytes (%d): %s\n", len(encoded5), hex.EncodeToString(encoded5))
	fmt.Println()
	
	// Output for validation comparison
	fmt.Println("=== VALIDATION REFERENCE ===")
	fmt.Printf("Standard [1.0, 2.0, 3.0]:     %s\n", hex.EncodeToString(encoded1))
	fmt.Printf("Edge [0.0, -1.0, +Inf]:       %s\n", hex.EncodeToString(encoded2))
	fmt.Printf("Single [3.14159]:             %s\n", hex.EncodeToString(encoded3))
	fmt.Printf("Large first 32:               %s\n", hex.EncodeToString(encoded4[:32]))
}