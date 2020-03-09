#pragma once

#include "engine/work_item.h"
#include "engine/worker_thread.h"
#include "engine/renderer/model.h"
#include "engine/renderer/memory.h"
#include "dat/mzb.h"

class CollisionModelInitTask : public lotus::WorkItem
{
public:
    CollisionModelInitTask(std::shared_ptr<lotus::Model> model, std::vector<FFXI::CollisionMeshData> mesh_data, std::vector<FFXI::CollisionEntry> entries, uint32_t vertex_stride);
    virtual ~CollisionModelInitTask() override = default;
    virtual void Process(lotus::WorkerThread*) override;
private:
    std::shared_ptr<lotus::Model> model;
    std::vector<FFXI::CollisionMeshData> mesh_data;
    std::vector<FFXI::CollisionEntry> entries;
    uint32_t vertex_stride;
    std::unique_ptr<lotus::Buffer> staging_buffer;
    vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
};
