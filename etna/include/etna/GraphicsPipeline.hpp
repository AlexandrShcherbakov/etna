#pragma once
#ifndef ETNA_GRAPHICS_PIPELINE_HPP_INCLUDED
#define ETNA_GRAPHICS_PIPELINE_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <etna/VertexInput.hpp>
#include <etna/PipelineBase.hpp>
#include <variant>

namespace etna
{

class PipelineManager;

class GraphicsPipeline : public PipelineBase
{
  friend class PipelineManager;
  GraphicsPipeline(PipelineManager* inOwner, PipelineId inId, ShaderProgramId inShaderProgramId)
    : PipelineBase(inOwner, inId, inShaderProgramId)
  {
  }
public:
  // Use PipelineManager to create pipelines
  GraphicsPipeline() = default;

  struct CreateInfo
  {
    // Specifies the format in which vertices are fed to this
    // pipeline's vertex shader
    VertexShaderInputDescription vertexShaderInput;
    
    // Specifies what type of primitives you want to draw:
    // triangles, lines, etc. Also specifies additional tricky
    // stuff that is mostly not needed for basic applications.
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyConfig =
      {
        .topology = vk::PrimitiveTopology::eTriangleList
      };

    // Tesselation stage configuration
    vk::PipelineTessellationStateCreateInfo tesselationConfig =
      {
        // Number of control points per tesselation patch
        .patchControlPoints = 4,
      };

    // Configuration for the rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizationConfig =
      {
        // How polygons should be drawn (filled, outline, only corner points?)
        .polygonMode = vk::PolygonMode::eFill,
        // Whether we need to skip drawing back/front faces of polygons
        .cullMode = vk::CullModeFlagBits::eNone,
        // Which face we consider to be the front of the polygon
        .frontFace = vk::FrontFace::eClockwise,
        // Width of lines when drawing lines/outlines
        .lineWidth = 1.f,
      };

    // Configuration for alpha blending.
    // Disabled and configured to a single color attachment by default.
    // If you are not trying to do (advanced) transparency techniques,
    // you do not need this.
    struct Blending
    {
      // Should contain an element for every color attachment that
      // your pixel shader will output to, i.e. for every output variable.
      std::vector<vk::PipelineColorBlendAttachmentState> attachments = 
        {
          vk::PipelineColorBlendAttachmentState
          {
            .blendEnable = false,
            // Which color channels should we write to?
            .colorWriteMask = vk::ColorComponentFlagBits::eR
              | vk::ColorComponentFlagBits::eG
              | vk::ColorComponentFlagBits::eB
              | vk::ColorComponentFlagBits::eA
          }
        };
      bool logicOpEnable = false;
      vk::LogicOp logicOp;
      std::array<float, 4> blendConstants {0, 0, 0, 0};
    } blendingConfig = {};

    vk::PipelineDepthStencilStateCreateInfo depthConfig = 
      {
        // Discard fragments that are covered by other fragments?
        .depthTestEnable = true,
        // Write fragments' depth into the buffer after they have been drawn?
        .depthWriteEnable = true,
        // How should we decide whether one fragment covers another one?
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
        // Max allowed depth, usually changed for tricky hacks
        .maxDepthBounds = 1.f,
      };

    // For the GPU driver to compile a SPIR-V shader into native GPU bytecode,
    // on almost all platforms it needs to know at least a little bit of info
    // about what the shader will be outputting it's result to, i.e. formats
    // of all attachments. vk::Format::eUndefined here means that the attachment
    // is not outputted to by this pipeline.
    struct FragmentShaderOutputDescription
    {
      std::vector<vk::Format> colorAttachmentFormats = {};
      vk::Format depthAttachmentFormat = vk::Format::eUndefined;
      vk::Format stencilAttachmentFormat = vk::Format::eUndefined;
    } fragmentShaderOutput;
  };
};

}


#endif // ETNA_GRAPHICS_PIPELINE_HPP_INCLUDED
