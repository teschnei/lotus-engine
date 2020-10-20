#include "ui.h"

#include <algorithm>
#include "engine/core.h"
#include "element.h"

namespace lotus::ui
{
    Manager::Manager(Engine* _engine) : engine(_engine) {}

    Task<> Manager::Init()
    {
        root = std::make_shared<Element>();

        root->SetWidth(engine->renderer->swapchain->extent.width);
        root->SetHeight(engine->renderer->swapchain->extent.height);
        root->bg_colour = glm::vec4(0.f);
        auto task = root->Init(engine, root);
        co_await task;
    }

    Task<> Manager::addElement(std::shared_ptr<Element> ele, std::shared_ptr<Element> parent)
    {
        auto work = ele->Init(engine, ele);
        if (!parent) parent = root;
        parent->AddChild(ele);
        co_await work;
    }

    std::vector<vk::CommandBuffer> Manager::getRenderCommandBuffers(int image_index)
    {
        std::vector<vk::CommandBuffer> buffers;
        root->GetCommandBuffers(std::back_inserter(buffers), image_index);
        return buffers;
    }
}
