module;

#include <memory>
#include <vector>

module lotus:renderer.vulkan.ui_renderer;

import :renderer.memory;
import :renderer.texture;
import :ui.element;
import :util;

import vulkan_hpp;

namespace lotus
{
export class Engine;
export class Renderer;
class UiRenderer
{
public:
    UiRenderer(Engine*, Renderer*);
    Task<> Init();
    Task<> ReInit();

    std::vector<vk::CommandBuffer> Render();
    void GenerateRenderBuffers(ui::Element*);

private:
    void createDescriptorSetLayout();
    void createDepthImage();
    void createPipeline();
    Task<> createBuffers();

    struct Quad
    {
        std::unique_ptr<Buffer> vertex_buffer;
    } quad;

    Engine* engine;
    Renderer* renderer;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;
    vk::UniqueDescriptorSetLayout descriptor_set_layout;

    std::unique_ptr<Image> depth_image;
    vk::UniqueImageView depth_image_view;

    std::shared_ptr<Texture> default_texture;
};
} // namespace lotus
