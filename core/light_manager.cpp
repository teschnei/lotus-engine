#include "light_manager.h"
#include "core.h"

namespace lotus
{
    LightManager::LightManager(Engine* _engine) : engine(_engine)
    {
        dir_buffer = engine->renderer.memory_manager->GetBuffer(sizeof(directional_light) * engine->renderer.getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    void LightManager::UpdateLightBuffer()
    {
        auto data = engine->renderer.device->mapMemory(dir_buffer->memory, dir_buffer->memory_offset, sizeof(directional_light), {}, engine->renderer.dispatch);
        memcpy(static_cast<uint8_t*>(data) + (0* sizeof(directional_light)), &directional_light, sizeof(directional_light));
        memcpy(static_cast<uint8_t*>(data) + (1* sizeof(directional_light)), &directional_light, sizeof(directional_light));
        memcpy(static_cast<uint8_t*>(data) + (2* sizeof(directional_light)), &directional_light, sizeof(directional_light));
        engine->renderer.device->unmapMemory(dir_buffer->memory, engine->renderer.dispatch);
    }
}
