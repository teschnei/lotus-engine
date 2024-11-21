#include "light_manager.h"

#include <ranges>

#include "core.h"
#include "renderer/vulkan/renderer.h"

namespace lotus
{
LightManager::LightManager(Engine* _engine) : engine(_engine)
{
    lights_buffer_count = 100;
    light_buffer =
        engine->renderer->gpu->memory_manager->GetBuffer(GetBufferSize() * engine->renderer->getFrameCount(), vk::BufferUsageFlagBits::eStorageBuffer,
                                                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    light_buffer_map = static_cast<uint8_t*>(light_buffer->map(0, GetBufferSize() * engine->renderer->getFrameCount(), {}));
}

LightManager::~LightManager() { light_buffer->unmap(); }

void LightManager::UpdateLightBuffer()
{
    light.light_count = lights.size();
    memcpy(light_buffer_map + (engine->renderer->getCurrentFrame() * GetBufferSize()), &light, sizeof(light));
    memcpy(light_buffer_map + (engine->renderer->getCurrentFrame() * GetBufferSize()) + sizeof(LightBuffer), lights.data(), lights.size() * sizeof(Light));
}

size_t LightManager::GetBufferSize() { return engine->renderer->uniform_buffer_align_up(sizeof(LightBuffer) + (sizeof(Light) * lights_buffer_count)); }

LightID LightManager::AddLight(Light light)
{
    std::lock_guard lock{light_buffer_mutex};
    if (lights.size() == lights_buffer_count)
    {
        lights_buffer_count = lights_buffer_count * 2;
        light_buffer->unmap();
        light_buffer =
            engine->renderer->gpu->memory_manager->GetBuffer(GetBufferSize() * engine->renderer->getFrameCount(), vk::BufferUsageFlagBits::eStorageBuffer,
                                                             vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        light_buffer_map = static_cast<uint8_t*>(light_buffer->map(0, GetBufferSize() * engine->renderer->getFrameCount(), {}));
    }
    light.id = cur_light_id++;
    lights.push_back(light);
    return light.id;
}

void LightManager::RemoveLight(LightID lid)
{
    std::lock_guard lock{light_buffer_mutex};
    std::erase_if(lights, [&lid](auto& l) { return l.id == lid; });
}

void LightManager::UpdateLight(LightID lid, Light light)
{
    light.id = lid;
    // TODO: multiple updates don't need to take the mutex, just for add/remove
    std::shared_lock lock{light_buffer_mutex};
    auto l = std::ranges::find(lights, lid, &Light::id);
    if (l != lights.end())
        *l = light;
}
} // namespace lotus
