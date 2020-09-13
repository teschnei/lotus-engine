#include "ui.h"

#include <algorithm>
#include "engine/core.h"
#include "element.h"

namespace lotus::ui
{
    Manager::Manager(Engine* _engine) : engine(_engine) {}

    std::vector<UniqueWork> Manager::Init()
    {
        root = std::make_shared<Element>();

        root->SetWidth(engine->renderer->swapchain->extent.width);
        root->SetHeight(engine->renderer->swapchain->extent.height);
        root->bg_colour = glm::vec4(0.f);

        return root->Init(root);
    }

    std::vector<UniqueWork> Manager::addElement(std::shared_ptr<Element> ele, std::shared_ptr<Element> parent)
    {
        auto work = ele->Init(ele);
        if (!parent) parent = root;
        parent->AddChild(ele);
        return work;
    }

    std::vector<vk::CommandBuffer> Manager::getRenderCommandBuffers(int image_index)
    {
        std::vector<vk::CommandBuffer> buffers;
        root->GetCommandBuffers(std::back_inserter(buffers), image_index);
        return buffers;
    }
}
