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
            glm::mat4 model_t;
            glm::mat3 model_it;
        };

        explicit LandscapeEntity(Engine* _engine) : RenderableEntity(_engine) {}
        virtual void populate_AS(TopLevelAccelerationStructure* as, uint32_t image_index) override;
        virtual void update_AS(TopLevelAccelerationStructure* as, uint32_t image_index) override;
        virtual WorkerTask<> ReInitWork() override;

        std::unique_ptr<Buffer> instance_buffer;
        std::vector<InstanceInfo> instance_info;
        std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets; //pair of offset/count

        std::vector<std::shared_ptr<Model>> collision_models;
        std::shared_ptr<TopLevelAccelerationStructure> collision_as;
    protected:
        WorkerTask<> InitWork(std::vector<InstanceInfo>&&);
    };
}
