#pragma once
#include "component.h"
#include <memory>
#include <vector>
#include "lotus/worker_task.h"
#include "lotus/renderer/model.h"

namespace lotus::Component
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

        struct ModelInfo
        {
            std::shared_ptr<Model> model;
            std::unique_ptr<GlobalDescriptors::MeshInfoBuffer::View> mesh_infos;
        };

        explicit InstancedModelsComponent(Entity*, Engine* engine, std::vector<std::shared_ptr<Model>> models,
            const std::vector<InstanceInfo>& instances, const std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets);

        WorkerTask<> init();

        std::span<const ModelInfo> getModels() const;
        vk::Buffer getInstanceBuffer() const;
        std::pair<vk::DeviceSize, uint32_t> getInstanceOffset(const std::string& name) const;
        InstanceInfo getInstanceInfo(vk::DeviceSize offset) const;

    protected:
        std::vector<ModelInfo> models;
        std::vector<InstanceInfo> instances;
        std::unique_ptr<Buffer> instance_buffer;
        std::unordered_map<std::string, std::pair<vk::DeviceSize, uint32_t>> instance_offsets; //pair of offset/count
    };
}
