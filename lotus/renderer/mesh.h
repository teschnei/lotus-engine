#pragma once

#include "lotus/renderer/vulkan/vulkan_inc.h"
#include "material.h"
#include "memory.h"
#include <memory>

namespace lotus {
class Engine;

class Mesh {
public:
  Mesh(const Mesh &) = delete;
  Mesh &operator=(const Mesh &) = delete;
  Mesh(Mesh &&) = default;
  Mesh &operator=(Mesh &&) = default;
  virtual ~Mesh() = default;

  std::vector<vk::VertexInputBindingDescription> &
  getVertexInputBindingDescription() {
    return vertex_bindings;
  }

  void setVertexInputBindingDescription(
      std::vector<vk::VertexInputBindingDescription> &&desc) {
    vertex_bindings = desc;
  }

  std::vector<vk::VertexInputAttributeDescription> &
  getVertexInputAttributeDescription() {
    return vertex_attributes;
  }

  void setVertexInputAttributeDescription(
      std::vector<vk::VertexInputAttributeDescription> &&attrs, size_t stride) {
    vertex_attributes = std::move(attrs);
    vertex_stride = stride;
  }

  size_t getVertexStride() { return vertex_stride; }

  int getIndexCount() const { return indices; }
  int getVertexCount() const { return vertices; }
  uint32_t getMaxIndex() const { return max_index; }
  uint16_t getSpriteCount() const { return sprite_count; }
  void setIndexCount(int _indices) { indices = _indices; }
  void setVertexCount(int _vertices) { vertices = _vertices; }
  void setMaxIndex(uint32_t _max_index) { max_index = _max_index; }
  void setSpriteCount(uint16_t _sprite_count) { sprite_count = _sprite_count; }

  void setVertexBuffer(uint8_t *buffer, size_t len);

  std::unique_ptr<Buffer> vertex_buffer;
  std::unique_ptr<Buffer> index_buffer;
  std::unique_ptr<Buffer> aabbs_buffer;

  std::shared_ptr<Material> material;

  bool has_transparency{false};
  uint16_t blending{0};

  Mesh() = default;

  std::vector<vk::Pipeline> pipelines;

protected:
  std::vector<vk::VertexInputBindingDescription> vertex_bindings;
  std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
  size_t vertex_stride{0};
  int indices{0};
  int vertices{0};
  uint32_t max_index{0};
  uint16_t sprite_count{1};
};
} // namespace lotus
