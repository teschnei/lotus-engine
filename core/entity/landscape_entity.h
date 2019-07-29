#pragma once
#include "renderable_entity.h"

namespace lotus
{
    class LandscapeEntity : public RenderableEntity
    {
    public:
        struct InstanceInfo
        {
            glm::mat4 matrix;
        };

        std::unique_ptr<Buffer> instance_buffer;
        std::unordered_map<std::string, std::pair<vk::DeviceSize, vk::DeviceSize>> instance_offsets; //pair of offset/count
        std::vector<std::pair<std::string, std::shared_ptr<Model>>> instance_models;
    };
}
