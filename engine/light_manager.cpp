#include "light_manager.h"
#include "core.h"
#include "renderer/vulkan/renderer.h"

namespace lotus
{
    LightManager::LightManager(Engine* _engine) : engine(_engine)
    {
        light_buffer = engine->renderer.gpu->memory_manager->GetBuffer(engine->renderer.uniform_buffer_align_up(sizeof(LightBuffer)) * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        buffer_map = static_cast<uint8_t*>(light_buffer->map(0, engine->renderer.uniform_buffer_align_up(sizeof(LightBuffer)) * engine->renderer.getImageCount(), {}));
    }

    LightManager::~LightManager()
    {
        light_buffer->unmap();
    }

    void LightManager::UpdateLightBuffer()
    {
        memcpy(buffer_map + (engine->renderer.getCurrentImage() * engine->renderer.uniform_buffer_align_up(sizeof(LightBuffer))), &light, sizeof(light));
    }
}
