#pragma once
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <engine/renderer/vulkan/vulkan_inc.h>
#include "engine/renderer/memory.h"
#include "engine/task.h"
#include "engine/worker_pool.h"

//class for doing generic raytracing queries
namespace lotus
{
    class Engine;
    class RaytraceQueryer
    {
    public:
        enum class ObjectFlags
        {
            LevelGeometry = 1,
            DynamicEntities = 2,
            LevelCollision = 4,
            LevelCollisionLOS = 8,
            Particle = 16,
            Water = 32
        };
        RaytraceQueryer(Engine* engine);
        Task<float> query(ObjectFlags object_flags, glm::vec3 origin, glm::vec3 direction, float min, float max);

    private:
        class RaytraceQuery
        {
        public:
            RaytraceQuery(ObjectFlags _object_flags, glm::vec3 _origin, glm::vec3 _direction, float _min, float _max) :
                object_flags(_object_flags), origin(_origin), direction(_direction), min(_min), max(_max) {}

            ObjectFlags object_flags;
            glm::vec3 origin;
            glm::vec3 direction;
            float min;
            float max;
            float result{};
        };
        struct RaytraceInput
        {
            glm::vec3 origin;
            float min;
            glm::vec3 direction;
            float max;
            uint32_t flags;
            float _pad[3];
        };
        struct RaytraceOutput
        {
            float intersection_dist;
            float _pad[3];
        };
        Engine* engine;

        static constexpr size_t max_queries{ 1024 };
        vk::Queue raytrace_query_queue;

        Task<float> query_queue(ObjectFlags object_flags, glm::vec3 origin, glm::vec3 direction, float min, float max);

        void runQueries();
        AsyncQueue<RaytraceQuery> async_query_queue;
        std::atomic<uint64_t> task_count;

        //RTX
        std::unique_ptr<Buffer> shader_binding_table;
        vk::UniqueHandle<vk::DescriptorSetLayout, vk::DispatchLoaderDynamic> rtx_descriptor_layout;
        vk::UniqueHandle<vk::DescriptorPool, vk::DispatchLoaderDynamic> rtx_descriptor_pool;
        vk::UniqueHandle<vk::DescriptorSet, vk::DispatchLoaderDynamic> rtx_descriptor_set;
        vk::UniqueHandle<vk::PipelineLayout, vk::DispatchLoaderDynamic> rtx_pipeline_layout;
        vk::UniqueHandle<vk::Pipeline, vk::DispatchLoaderDynamic> rtx_pipeline;
        vk::StridedDeviceAddressRegionKHR raygenSBT;
        vk::StridedDeviceAddressRegionKHR missSBT;
        vk::StridedDeviceAddressRegionKHR hitSBT;
        vk::UniqueHandle<vk::Fence, vk::DispatchLoaderDynamic> fence;
        vk::UniqueHandle<vk::CommandPool, vk::DispatchLoaderDynamic> command_pool;
        std::unique_ptr<Buffer> input_buffer;
        std::unique_ptr<Buffer> output_buffer;
        //
    };
}
