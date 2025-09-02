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

#ifndef DATASTAX_INTERNAL_VECTOR_TYPE_HPP
#define DATASTAX_INTERNAL_VECTOR_TYPE_HPP

#include "data_type.hpp"
#include "string.hpp"

namespace datastax { namespace internal { namespace core {

/**
 * VectorType represents the type definition for Cassandra vector columns.
 * Vectors are CUSTOM types with a fixed dimension and element type.
 * Type string: org.apache.cassandra.db.marshal.VectorType(ElementType, Dimension)
 */
class VectorType : public CustomType {
public:
  typedef SharedRefPtr<const VectorType> ConstPtr;
  
  static const char* VECTOR_CLASS_NAME;
  
  /**
   * Create a new vector type with the specified element type and dimension.
   * 
   * @param element_type The type of elements in the vector
   * @param dimension The fixed number of elements (1-8192)
   */
  VectorType(const DataType::ConstPtr& element_type, size_t dimension)
      : CustomType()
      , element_type_(element_type)
      , dimension_(dimension) {
    update_class_name();
  }
  
  /**
   * Parse a vector type from a custom type class name.
   * Expected format: org.apache.cassandra.db.marshal.VectorType(ElementType, Dimension)
   * 
   * @param class_name The full class name to parse
   * @return A vector type or nullptr if parsing fails
   */
  static ConstPtr from_class_name(const String& class_name);
  
  const DataType::ConstPtr& element_type() const { return element_type_; }
  size_t dimension() const { return dimension_; }
  
  void set_element_type(const DataType::ConstPtr& element_type) {
    element_type_ = element_type;
    update_class_name();
  }
  
  void set_dimension(size_t dimension) {
    dimension_ = dimension;
    update_class_name();
  }
  
  /**
   * Check if the element type is fixed-length (no UVINT prefix needed).
   * Fixed-length types: bigint, boolean, timestamp, double, float, int, timeuuid, uuid
   */
  bool is_fixed_length_element() const;
  
  virtual bool equals(const DataType::ConstPtr& data_type) const;
  virtual DataType::Ptr copy() const;
  virtual String to_string() const;
  
private:
  void update_class_name();
  
private:
  DataType::ConstPtr element_type_;
  size_t dimension_;
};

}}} // namespace datastax::internal::core

#endif // DATASTAX_INTERNAL_VECTOR_TYPE_HPP