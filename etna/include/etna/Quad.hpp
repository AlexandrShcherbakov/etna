#pragma once
#ifndef ETNA_QUAD_HPP_INCLUDED
#define ETNA_QUAD_HPP_INCLUDED

#include <vector>
#include <string>
#include <etna/Vulkan.hpp>
#include "etna/GraphicsPipeline.hpp"
#include "etna/Image.hpp"
#include "etna/Sampler.hpp"


namespace etna
{

class QuadRenderer
{
public:
  QuadRenderer(int32_t startX, int32_t startY, uint32_t sizeX, uint32_t sizeY) 
  {
    m_rect.offset = {startX, startY};
    m_rect.extent = {sizeX, sizeY};
  }
  virtual ~QuadRenderer() {}

  struct CreateInfo
  {
    vk::Format format = vk::Format::eUndefined;
  };

  void Create(const char *vspath, const char *fspath, CreateInfo info);
  void DrawCmd(VkCommandBuffer cmdBuff, VkImage targetImage, VkImageView targetImageView,
               const Image &inTex, const Sampler &sampler);

private:
  GraphicsPipeline m_pipeline;
  ShaderProgramId m_programId;
  vk::Rect2D m_rect {};

  QuadRenderer(const QuadRenderer &) = delete;
  QuadRenderer& operator=(const QuadRenderer &) = delete;
};

}

#endif // ETNA_QUAD_HPP_INCLUDED
