#include "ui_element_init.h"
#include "engine/core.h"
#include "engine/worker_thread.h"
#include "engine/renderer/vulkan/renderer.h"
#include "engine/renderer/vulkan/gpu.h"

namespace lotus
{
    UiElementInitTask::UiElementInitTask(std::shared_ptr<ui::Element> _element) : element(_element)
    {
    }

    void UiElementInitTask::Process(WorkerThread* thread)
    {
        element->buffer = thread->engine->renderer->gpu->memory_manager->GetBuffer(thread->engine->renderer->uniform_buffer_align_up(sizeof(ui::Element::UniformBuffer)) * thread->engine->renderer->getImageCount(), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        uint8_t* buffer_mapped = static_cast<uint8_t*>(element->buffer->map(0, thread->engine->renderer->uniform_buffer_align_up(sizeof(ui::Element::UniformBuffer)) * thread->engine->renderer->getImageCount(), {}));
        for (size_t i = 0; i < thread->engine->renderer->getImageCount(); ++i)
        {
            auto pos = element->GetAbsolutePos();
            auto buf = reinterpret_cast<ui::Element::UniformBuffer*>(buffer_mapped);
            buf->x = pos.x;
            buf->y = pos.y;
            buf->width = element->GetWidth();
            buf->height = element->GetHeight();
            buf->bg_colour = element->bg_colour;
            buf->alpha = element->alpha;
            buffer_mapped += thread->engine->renderer->uniform_buffer_align_up(sizeof(ui::Element::UniformBuffer));
        }
        element->buffer->unmap();

        vk::CommandBufferAllocateInfo alloc_info;
        alloc_info.level = vk::CommandBufferLevel::eSecondary;
        alloc_info.commandPool = *thread->graphics_pool;
        alloc_info.commandBufferCount = static_cast<uint32_t>(thread->engine->renderer->getImageCount());

        element->command_buffers = thread->engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

        thread->engine->renderer->ui->GenerateRenderBuffers(element);
    }
}
