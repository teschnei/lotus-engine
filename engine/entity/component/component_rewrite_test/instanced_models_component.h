#pragma once
#include "component.h"
#include <memory>
#include <vector>
#include "animation_component.h"
#include "engine/worker_task.h"
#include "engine/renderer/model.h"

namespace lotus::Test
{
    class InstancedModelsComponent : public Component<InstancedModelsComponent>
    {
    public:
        struct InstanceInfo
        {
            glm::mat4 model;
            glm::mat4 model_t;
            glm::mat3 model_it;
        };

        explicit InstancedModelsComponent(Entity*, Engine* engine, std::vector<std::shared_ptr<Model>> models,
            const std::vector<InstanceInfo>& instances, const std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets);

        WorkerTask<> init();
        Task<> tick(time_point time, duration delta);

        std::vector<std::shared_ptr<Model>> getModels() const;
        vk::Buffer getInstanceBuffer() const;
        std::pair<vk::DeviceSize, uint32_t> getInstanceOffset(const std::string& name) const;
        InstanceInfo getInstanceInfo(vk::DeviceSize offset) const;

    protected:
        std::vector<std::shared_ptr<Model>> models;
        std::vector<InstanceInfo> instances;
        std::unique_ptr<Buffer> instance_buffer;
        std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets; //pair of offset/count
    };
}
