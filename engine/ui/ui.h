#pragma once

#include <vector>
#include <memory>
#include "element.h"

namespace lotus
{
    class Engine;

    namespace ui
    {
        class Element;
        class Manager
        {
        public:
            Manager(Engine* engine);

            [[nodiscard("Work must be queued in order to be processed")]]
            std::vector<UniqueWork> Init();

            [[nodiscard("Work must be queued in order to be processed")]]
            std::vector<UniqueWork> addElement(std::shared_ptr<Element>, std::shared_ptr<Element> parent = nullptr);

            std::vector<vk::CommandBuffer> getRenderCommandBuffers(int image_index);
        private:
            Engine* engine;
            std::shared_ptr<Element> root;
        };
    }
}
