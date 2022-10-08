#pragma once
#ifndef ETNA_VERTEX_INPUT_HPP_INCLUDED
#define ETNA_VERTEX_INPUT_HPP_INCLUDED

#include <optional>

#include <etna/Vulkan.hpp>


namespace etna
{

// This structure describes how the vertex shader should interpret
// a stream of bytes.
// NOTE: The intention here is for model loading facilities to provide
// this object to rendering facilities
struct VertexByteStreamFormatDescription
{
  // The size of a single vertex in bytes. You are not required to use
  // all bytes specified here, i.e. vertices may contain padding.
  uint32_t stride;

  struct Attribute
  {
    // The byte format of this attribute. As an example, R8G8B8A8_UNORM
    // would mean a 4 component vector which is encoded using 8-bit unsigned
    // fixed precision numbers (i.e. each byte is interpreted as an unsigned 
    // integer from 0 to 255, converted to float and divided by 255).
    // For most purposes you should use R32G32B32A32_SFLOAT -- 32-bit signed
    // floating point numbers.
    vk::Format format;
    // Offset from start of vertex bytes for this attribute
    uint32_t offset;
  };

  // Each vertex may contain multiple attributes, e.g. position, normal and UV coords
  std::vector<Attribute> attributes;

  std::vector<uint32_t> identityAttributeMapping() const
  {
    std::vector<uint32_t> result(attributes.size());
    for (uint32_t i = 0; i < result.size(); ++i)
      result[i] = i;
    return result;
  }
};

struct VertexShaderInputDescription
{
  // Vulkan supports multiple vertex buffer at the same time.
  // The input variables you define in GLSL shaders have a `binding`
  // annotation, which is 0 by default, and determines which index buffer
  // slot will be used for these variables.
  struct Binding
  {
    // Description of the byte stream (i.e. vertex buffer) that will be used
    // for this binding. It might not make sense that this data must be specified
    // at pipeline creation time, but most hardware requires this information
    // to compile SPIR-V code into GPU bytecode, so a pipeline cannot be created
    // without knowing how bytes will be fed into it.
    VertexByteStreamFormatDescription byteStreamDescription;

    // How often should variables inside this binding be updated
    // with a new value? For every "vertex" or every "instance"?
    vk::VertexInputRate inputRate = vk::VertexInputRate::eVertex;

    // For every input variable inside your GLSL vertex shader with `location = i`,
    // you should have `attributeMapping[i]` saying which attribute from the
    // byte stream description should be used for this variable.
    // Default is identity i -> i mapping.
    std::vector<uint32_t> attributeMapping = byteStreamDescription.identityAttributeMapping();
  };
  
  // Note that the `binding` annotation value that you specified in GLSL
  // will be used to index this array. For most use cases, a single element
  // will be enough.
  std::vector<std::optional<Binding>> bindings;
};

}

#endif // ETNA_VERTEX_INPUT_HPP_INCLUDED
