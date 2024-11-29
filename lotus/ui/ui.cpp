module;

#include <coroutine>
#include <memory>
#include <vector>

module lotus;

import :ui;

import :ui.element;
import :core.engine;
import :renderer.vulkan.renderer;
import :util;
import glm;
import vulkan_hpp;

namespace lotus::ui
{
Manager::Manager(Engine* _engine) : engine(_engine) {}

Task<> Manager::Init()
{
    root = std::make_shared<Element>();

    root->SetWidth(engine->renderer->swapchain->extent.width);
    root->SetHeight(engine->renderer->swapchain->extent.height);
    root->bg_colour = glm::vec4(0.f);
    co_await root->Init(engine, root);
}

Task<> Manager::ReInit()
{
    root->ReInit(engine);
    co_return;
}

Task<> Manager::addElement(std::shared_ptr<Element> ele, std::shared_ptr<Element> parent)
{
    auto work = ele->Init(engine, ele);
    if (!parent)
        parent = root;
    parent->AddChild(ele);
    co_await work;
}

std::vector<vk::CommandBuffer> Manager::getRenderCommandBuffers(int image_index)
{
    std::vector<vk::CommandBuffer> buffers;
    root->GetCommandBuffers(std::back_inserter(buffers), image_index);
    return buffers;
}
} // namespace lotus::ui
