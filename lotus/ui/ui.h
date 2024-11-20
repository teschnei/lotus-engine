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

            Task<> Init();
            Task<> ReInit();
            Task<> addElement(std::shared_ptr<Element>, std::shared_ptr<Element> parent = nullptr);

            std::vector<vk::CommandBuffer> getRenderCommandBuffers(int image_index);
        private:
            Engine* engine;
            std::shared_ptr<Element> root;
        };
    }
}
