module;

#include <memory>
#include <utility>

export module lotus:renderer.material;

import :renderer.memory;
import :renderer.texture;
import :renderer.vulkan.common.global_descriptors;
import :util;
import glm;
import vulkan_hpp;

export namespace lotus
{
class Material
{
public:
    static WorkerTask<std::shared_ptr<Material>> make_material(Engine* engine, std::shared_ptr<Buffer> buffer, uint32_t buffer_offset,
                                                               std::shared_ptr<Texture> texture, uint32_t light_type = 0, glm::vec2 roughness = glm::vec2(1),
                                                               float ior = 0.f);

    const std::shared_ptr<Texture> texture;
    const glm::vec2 roughness;
    const float ior;
    const uint32_t light_type;
    size_t index{0};

    std::pair<vk::Buffer, uint32_t> getBuffer() { return {buffer->buffer, buffer_offset}; }
    static size_t getMaterialBufferSize(Engine*);

private:
    Material(std::shared_ptr<Buffer>, uint32_t buffer_offset, std::shared_ptr<Texture> texture, uint32_t light_type, glm::vec2 roughness, float ior);
    std::shared_ptr<Buffer> buffer;
    uint32_t buffer_offset;
};
} // namespace lotus
