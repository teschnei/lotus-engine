#include "element.h"
#include "glm/gtc/matrix_transform.hpp"
#include "lotus/core.h"
#include "lotus/renderer/vulkan/gpu.h"
#include "lotus/renderer/vulkan/renderer.h"

namespace lotus::ui
{
Element::Element() {}

Task<> Element::Init(Engine* engine, std::shared_ptr<Element> self) { co_await self->InitWork(engine); }

void Element::ReInit(Engine* engine)
{
    GenerateCommandBuffers(engine);
    for (const auto& e : children)
    {
        e->ReInit(engine);
    }
}

WorkerTask<> Element::InitWork(Engine* engine)
{
    buffer = engine->renderer->gpu->memory_manager->GetBuffer(
        engine->renderer->uniform_buffer_align_up(sizeof(ui::Element::UniformBuffer)) * engine->renderer->getImageCount(),
        vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    uint8_t* buffer_mapped = static_cast<uint8_t*>(
        buffer->map(0, engine->renderer->uniform_buffer_align_up(sizeof(ui::Element::UniformBuffer)) * engine->renderer->getImageCount(), {}));
    for (size_t i = 0; i < engine->renderer->getImageCount(); ++i)
    {
        auto pos = GetAbsolutePos();
        auto buf = reinterpret_cast<ui::Element::UniformBuffer*>(buffer_mapped);
        buf->x = pos.x;
        buf->y = pos.y;
        buf->width = GetWidth();
        buf->height = GetHeight();
        buf->bg_colour = bg_colour;
        buf->alpha = alpha;
        buffer_mapped += engine->renderer->uniform_buffer_align_up(sizeof(ui::Element::UniformBuffer));
    }
    buffer->unmap();

    GenerateCommandBuffers(engine);
    co_return;
}

void Element::GenerateCommandBuffers(Engine* engine)
{
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandPool = *engine->renderer->graphics_pool;
    alloc_info.commandBufferCount = static_cast<uint32_t>(engine->renderer->getImageCount());

    command_buffers = engine->renderer->gpu->device->allocateCommandBuffersUnique(alloc_info);

    engine->renderer->ui->GenerateRenderBuffers(this);
}

glm::ivec2 Element::GetAbsolutePos() { return screen_pos; }

glm::ivec2 Element::GetRelativePos() { return pos; }

glm::ivec2 Element::GetAnchorOffset(AnchorPoint anchor)
{
    glm::ivec2 pos{};

    if (anchor == AnchorPoint::TopRight || anchor == AnchorPoint::Right || anchor == AnchorPoint::BottomRight)
    {
        pos.x = width;
    }
    else if (anchor == AnchorPoint::Top || anchor == AnchorPoint::Center || anchor == AnchorPoint::Bottom)
    {
        pos.x = (width / 2);
    }

    if (anchor == AnchorPoint::BottomLeft || anchor == AnchorPoint::Bottom || anchor == AnchorPoint::BottomRight)
    {
        pos.y = height;
    }
    else if (anchor == AnchorPoint::Left || anchor == AnchorPoint::Center || anchor == AnchorPoint::Right)
    {
        pos.y = (height / 2);
    }
    return pos;
}

uint32_t Element::GetWidth() { return width; }

uint32_t Element::GetHeight() { return height; }

void Element::SetPos(glm::ivec2 _pos)
{
    pos = _pos;
    RecalculateScreenPos();
}

void Element::SetWidth(uint32_t _width)
{
    width = _width;
    RecalculateScreenPos();
}

void Element::SetHeight(uint32_t _height)
{
    height = _height;
    RecalculateScreenPos();
}

void Element::RecalculateScreenPos()
{
    if (parent)
    {
        RecalculateScreenPosFromParent(parent->GetAbsolutePos());
        for (auto& child : children)
        {
            child->RecalculateScreenPosFromParent(screen_pos);
        }
    }
}

void Element::RecalculateScreenPosFromParent(glm::ivec2 parent_pos)
{
    screen_pos = parent_pos + parent->GetAnchorOffset(parent_anchor) + pos - GetAnchorOffset(anchor);
}

void Element::AddChild(std::shared_ptr<Element> child)
{
    child->parent = this;
    auto iter = std::ranges::upper_bound(children, child->GetZ(), {}, &std::shared_ptr<Element>::element_type::GetZ);
    children.insert(iter, child);
    child->RecalculateScreenPosFromParent(screen_pos);
}
} // namespace lotus::ui
