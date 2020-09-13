#pragma once

#include "engine/types.h"
#include <vector>
#include <iterator>
#include <glm/glm.hpp>
#include "engine/renderer/vulkan/vulkan_inc.h"
#include "engine/renderer/texture.h"

namespace lotus::ui
{
    class Element
    {
    public:
        enum class AnchorPoint
        {
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight,
            Top,
            Bottom,
            Left,
            Right,
            Center
        };
        Element();
        std::vector<UniqueWork> Init(std::shared_ptr<Element>);

        glm::ivec2 GetRelativePos();
        glm::ivec2 GetAbsolutePos();
        uint32_t GetWidth();
        uint32_t GetHeight();
        int GetZ() {return z;};

        void SetPos(glm::ivec2 _pos);
        void SetWidth(uint32_t width);
        void SetHeight(uint32_t height);
        void AddChild(std::shared_ptr<Element>);

        void GetCommandBuffers(std::weakly_incrementable auto it, int image_index)
            requires std::indirectly_writable<decltype(it), typename vk::CommandBuffer>
        {
            *it = *command_buffers[image_index];
            for (const auto& e : children)
            {
                e->GetCommandBuffers(it, image_index);
            }
        }

        glm::vec4 bg_colour{};
        float alpha{1.f};

        std::vector<vk::UniqueCommandBuffer> command_buffers;
        std::shared_ptr<Texture> texture;

        struct UniformBuffer
        {
            uint32_t x;
            uint32_t y;
            uint32_t width;
            uint32_t height;
            glm::vec4 bg_colour;
            float alpha;
        };

        std::unique_ptr<Buffer> buffer;

        AnchorPoint anchor {AnchorPoint::TopLeft};
        AnchorPoint parent_anchor {AnchorPoint::TopLeft};

    private:
        glm::ivec2 GetAnchorOffset(AnchorPoint);
        void RecalculateScreenPos();
        void RecalculateScreenPosFromParent(glm::ivec2 parent);

        glm::ivec2 pos{};
        glm::ivec2 screen_pos{};
        int width {0};
        int height {0};
        int z {0};
        Element* parent {nullptr};
        std::vector<std::shared_ptr<Element>> children;
    };
}
