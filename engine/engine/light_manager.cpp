#include "light_manager.h"
#include "core.h"
#include "renderer/vulkan/renderer.h"

namespace lotus
{
    LightManager::LightManager(Engine* _engine) : engine(_engine)
    {
        light_buffer = engine->renderer.memory_manager->GetBuffer(sizeof(LightBuffer) * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        buffer_map = static_cast<LightBuffer*>(light_buffer->map(0, sizeof(light) * engine->renderer.getImageCount(), {}));
    }

    LightManager::~LightManager()
    {
        light_buffer->unmap();
    }

    void LightManager::UpdateLightBuffer()
    {
        memcpy(buffer_map + engine->renderer.getCurrentImage(), &light, sizeof(light));
    }
}
