module;

#include <memory>
#include <vector>

export module lotus:ui;

import :ui.element;
import :util;
import vulkan_hpp;

export namespace lotus
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
} // namespace ui
} // namespace lotus
