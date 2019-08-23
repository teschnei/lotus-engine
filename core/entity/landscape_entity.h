#pragma once
#include "renderable_entity.h"

namespace lotus
{
    class LandscapeEntity : public RenderableEntity
    {
    public:
        struct InstanceInfo
        {
            glm::mat4 model;
            glm::mat3 model_it;
        };

        std::unique_ptr<Buffer> instance_buffer;
        std::unordered_map<std::string, std::pair<vk::DeviceSize, vk::DeviceSize>> instance_offsets; //pair of offset/count
    };
}
