#include "light_manager.h"
#include "core.h"
#include "renderer/vulkan/renderer.h"

namespace lotus
{
    LightManager::LightManager(Engine* _engine) : engine(_engine)
    {
        dir_buffer = engine->renderer.memory_manager->GetBuffer(sizeof(LightBuffer) * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    void LightManager::UpdateLightBuffer()
    {
        auto data = dir_buffer->map(0, sizeof(light), {});
        memcpy(static_cast<uint8_t*>(data) + (engine->renderer.getCurrentImage() * sizeof(light)), &light, sizeof(light));
        dir_buffer->unmap();
    }
}
