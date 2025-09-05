/*
 * Unit test to demonstrate the vector type parsing issue with complex element types
 * 
 * This test shows that when Cassandra sends metadata for vector<list<int>, 2>,
 * the C++ driver fails to properly parse the Java long-form type format.
 */

#include <gtest/gtest.h>

#include "data_type_parser.hpp"
#include "vector_type.hpp"
#include "collection.hpp"
#include "logger.hpp"

using namespace datastax::internal;
using namespace datastax::internal::core;

class VectorTypeParsingTest : public ::testing::Test {
protected:
  void SetUp() {
    // Disable logging for cleaner test output
    cass_log_set_level(CASS_LOG_DISABLED);
  }
  
  void TearDown() {
    // Re-enable logging
    cass_log_set_level(CASS_LOG_WARN);
  }
};

TEST_F(VectorTypeParsingTest, ParseSimpleVectorType) {
  // Test parsing a simple vector type: vector<int, 3>
  // This simulates what we get for simple types (this should work)
  
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.Int32Type, 3)";
  
  VectorType::ConstPtr vector = VectorType::from_class_name(class_name);
  
  ASSERT_TRUE(vector) << "Failed to parse simple vector type";
  EXPECT_EQ(vector->dimension(), 3u) << "Wrong dimension";
  
  ASSERT_TRUE(vector->element_type()) << "No element type";
  EXPECT_EQ(vector->element_type()->value_type(), CASS_VALUE_TYPE_INT) 
    << "Element type should be INT";
}

TEST_F(VectorTypeParsingTest, ParseComplexVectorWithList) {
  // Test parsing vector<list<int>, 2>
  // This is what Cassandra actually sends for complex types
  
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.ListType("
                      "org.apache.cassandra.db.marshal.Int32Type), 2)";
  
  VectorType::ConstPtr vector = VectorType::from_class_name(class_name);
  
  ASSERT_TRUE(vector) << "Failed to parse vector with list element type";
  EXPECT_EQ(vector->dimension(), 2u) << "Wrong dimension";
  
  ASSERT_TRUE(vector->element_type()) << "No element type";
  
  // THIS IS THE BUG: The element type becomes CUSTOM with class_name "unknown"
  // instead of LIST with Int32Type elements
  
  CassValueType element_value_type = vector->element_type()->value_type();
  
  if (element_value_type == CASS_VALUE_TYPE_CUSTOM) {
    // This is what currently happens - the bug
    const CustomType* custom = static_cast<const CustomType*>(vector->element_type().get());
    std::cout << "BUG DETECTED: Element type is CUSTOM with class_name: '" 
              << custom->class_name() << "'" << std::endl;
    
    EXPECT_NE(custom->class_name(), "unknown") 
      << "Parser failed - created CustomType('unknown') instead of ListType";
  } else if (element_value_type == CASS_VALUE_TYPE_LIST) {
    // This is what should happen
    std::cout << "SUCCESS: Element type correctly parsed as LIST" << std::endl;
    
    const CollectionType* collection = 
      static_cast<const CollectionType*>(vector->element_type().get());
    
    ASSERT_EQ(collection->types().size(), 1u) << "List should have one element type";
    EXPECT_EQ(collection->types()[0]->value_type(), CASS_VALUE_TYPE_INT)
      << "List element should be INT";
  } else {
    FAIL() << "Unexpected element type: " << element_value_type;
  }
}

TEST_F(VectorTypeParsingTest, ParseComplexVectorWithSet) {
  // Test parsing vector<set<text>, 2>
  
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.SetType("
                      "org.apache.cassandra.db.marshal.UTF8Type), 2)";
  
  VectorType::ConstPtr vector = VectorType::from_class_name(class_name);
  
  ASSERT_TRUE(vector) << "Failed to parse vector with set element type";
  EXPECT_EQ(vector->dimension(), 2u) << "Wrong dimension";
  
  ASSERT_TRUE(vector->element_type()) << "No element type";
  
  CassValueType element_value_type = vector->element_type()->value_type();
  
  if (element_value_type == CASS_VALUE_TYPE_CUSTOM) {
    const CustomType* custom = static_cast<const CustomType*>(vector->element_type().get());
    std::cout << "BUG: Set parsed as CUSTOM with class_name: '" 
              << custom->class_name() << "'" << std::endl;
    
    EXPECT_NE(custom->class_name(), "unknown")
      << "Parser failed - created CustomType('unknown') instead of SetType";
  } else {
    EXPECT_EQ(element_value_type, CASS_VALUE_TYPE_SET)
      << "Element type should be SET";
  }
}

TEST_F(VectorTypeParsingTest, ParseComplexVectorWithMap) {
  // Test parsing vector<map<int, text>, 2>
  
  String class_name = "org.apache.cassandra.db.marshal.VectorType("
                      "org.apache.cassandra.db.marshal.MapType("
                      "org.apache.cassandra.db.marshal.Int32Type, "
                      "org.apache.cassandra.db.marshal.UTF8Type), 2)";
  
  VectorType::ConstPtr vector = VectorType::from_class_name(class_name);
  
  ASSERT_TRUE(vector) << "Failed to parse vector with map element type";
  EXPECT_EQ(vector->dimension(), 2u) << "Wrong dimension";
  
  ASSERT_TRUE(vector->element_type()) << "No element type";
  
  CassValueType element_value_type = vector->element_type()->value_type();
  
  if (element_value_type == CASS_VALUE_TYPE_CUSTOM) {
    const CustomType* custom = static_cast<const CustomType*>(vector->element_type().get());
    std::cout << "BUG: Map parsed as CUSTOM with class_name: '" 
              << custom->class_name() << "'" << std::endl;
    
    EXPECT_NE(custom->class_name(), "unknown")
      << "Parser failed - created CustomType('unknown') instead of MapType";
  } else {
    EXPECT_EQ(element_value_type, CASS_VALUE_TYPE_MAP)
      << "Element type should be MAP";
  }
}

TEST_F(VectorTypeParsingTest, DirectParserTestJavaFormat) {
  // Test the parser directly with Java long-form format
  // This shows exactly where the problem is
  
  SimpleDataTypeCache cache;
  
  // This is what VectorType::from_class_name() extracts and passes to the parser
  String element_type_str = "org.apache.cassandra.db.marshal.ListType("
                           "org.apache.cassandra.db.marshal.Int32Type)";
  
  std::cout << "Parsing element type string: " << element_type_str << std::endl;
  
  DataType::ConstPtr element_type = DataTypeClassNameParser::parse_one(element_type_str, cache);
  
  ASSERT_TRUE(element_type) << "Parser returned null for ListType";
  
  CassValueType value_type = element_type->value_type();
  
  if (value_type == CASS_VALUE_TYPE_CUSTOM) {
    const CustomType* custom = static_cast<const CustomType*>(element_type.get());
    std::cout << "PARSER BUG CONFIRMED: Returned CustomType with class_name: '"
              << custom->class_name() << "'" << std::endl;
    std::cout << "This is why complex vectors fail!" << std::endl;
    
    FAIL() << "Parser created CustomType('" << custom->class_name() 
           << "') instead of ListType for Java format";
  } else if (value_type == CASS_VALUE_TYPE_LIST) {
    std::cout << "Parser correctly returned LIST type" << std::endl;
  } else {
    FAIL() << "Parser returned unexpected type: " << value_type;
  }
}

TEST_F(VectorTypeParsingTest, CompareAngleBracketVsParentheses) {
  // Compare parsing of angle-bracket format (works) vs parentheses format (fails)
  
  SimpleDataTypeCache cache;
  
  // Angle bracket format - this is what the parser expects
  String angle_bracket_format = "org.apache.cassandra.db.marshal.ListType<"
                                "org.apache.cassandra.db.marshal.Int32Type>";
  
  // Parentheses format - this is what Cassandra actually sends
  String parentheses_format = "org.apache.cassandra.db.marshal.ListType("
                              "org.apache.cassandra.db.marshal.Int32Type)";
  
  std::cout << "\nTesting angle bracket format: " << angle_bracket_format << std::endl;
  DataType::ConstPtr angle_result = DataTypeClassNameParser::parse_one(angle_bracket_format, cache);
  
  std::cout << "Testing parentheses format: " << parentheses_format << std::endl;
  DataType::ConstPtr paren_result = DataTypeClassNameParser::parse_one(parentheses_format, cache);
  
  // Angle bracket format should work
  if (angle_result) {
    std::cout << "✓ Angle bracket format parsed successfully as type: " 
              << angle_result->value_type() << std::endl;
    EXPECT_EQ(angle_result->value_type(), CASS_VALUE_TYPE_LIST);
  } else {
    std::cout << "✗ Angle bracket format failed to parse" << std::endl;
  }
  
  // Parentheses format will fail
  if (paren_result) {
    CassValueType paren_type = paren_result->value_type();
    if (paren_type == CASS_VALUE_TYPE_CUSTOM) {
      const CustomType* custom = static_cast<const CustomType*>(paren_result.get());
      std::cout << "✗ Parentheses format incorrectly parsed as CustomType('"
                << custom->class_name() << "')" << std::endl;
      FAIL() << "Parentheses format should parse as LIST, not CustomType";
    } else if (paren_type == CASS_VALUE_TYPE_LIST) {
      std::cout << "✓ Parentheses format correctly parsed as LIST" << std::endl;
    }
  } else {
    std::cout << "✗ Parentheses format failed to parse" << std::endl;
    FAIL() << "Parser should handle parentheses format";
  }
}