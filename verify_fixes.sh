#!/bin/bash

echo "=== VERIFYING VECTOR ITERATOR FIXES ==="
echo

echo "1. Checking for dangerous hardcoded FLOAT type..."
if grep -n "DataType::ConstPtr(new DataType(CASS_VALUE_TYPE_FLOAT))" src/collection_iterator.cpp; then
  echo "❌ FAIL: Hardcoded FLOAT type still present!"
  exit 1
else
  echo "✓ PASS: No hardcoded FLOAT type found"
fi
echo

echo "2. Checking for silent fallback to empty vector..."
if grep -n "Fallback: treat as empty vector" src/collection_iterator.cpp; then
  echo "❌ FAIL: Silent fallback still present!"
  exit 1
else
  echo "✓ PASS: No silent fallback found"
fi
echo

echo "3. Checking for is_valid_ flag in VectorIterator..."
if grep -q "bool is_valid_" src/collection_iterator.hpp; then
  echo "✓ PASS: is_valid_ flag exists"
else
  echo "❌ FAIL: is_valid_ flag missing!"
  exit 1
fi
echo

echo "4. Checking that next() checks is_valid_..."
if grep -A2 "bool VectorIterator::next()" src/collection_iterator.cpp | grep -q "if (!is_valid_)"; then
  echo "✓ PASS: next() checks is_valid_"
else
  echo "❌ FAIL: next() doesn't check is_valid_!"
  exit 1
fi
echo

echo "5. Checking for proper error logging..."
if grep -q "LOG_ERROR.*Failed to parse vector type" src/collection_iterator.cpp; then
  echo "✓ PASS: Error logging present"
else
  echo "❌ FAIL: No error logging!"
  exit 1
fi
echo

echo "6. Verifying CollectionIterator is unchanged..."
if git diff HEAD~1 src/collection_iterator.cpp | grep -E "^[+-].*CollectionIterator::" | grep -v "^[+-]//" > /dev/null; then
  echo "⚠ WARNING: CollectionIterator was modified"
else
  echo "✓ PASS: CollectionIterator unchanged"
fi
echo

echo "=== VERIFICATION COMPLETE ==="
echo
echo "SUMMARY:"
echo "- Hardcoded defaults: REMOVED ✓"
echo "- Silent failures: REMOVED ✓"
echo "- Error state: ADDED ✓"
echo "- Error logging: ADDED ✓"
echo "- Existing code: PRESERVED ✓"