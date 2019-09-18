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

        virtual void populate_AS(TopLevelAccelerationStructure* as) override;
        virtual void update_AS(TopLevelAccelerationStructure* as) override;

        std::unique_ptr<Buffer> instance_buffer;
        std::vector<InstanceInfo> instance_info;
        std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets; //pair of offset/count
    };
}
