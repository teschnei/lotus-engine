#pragma once
#include <thread>
#include <engine/renderer/vulkan/vulkan_inc.h>
#include "work_item.h"

namespace lotus
{
    class Engine;
    class WorkerPool;
    class WorkerThread 
    {
    public:
        WorkerThread(Engine*, WorkerPool*);
        WorkerThread(const WorkerThread&) = delete;
        WorkerThread(WorkerThread&&) = delete;
        WorkerThread& operator=(const WorkerThread&) = delete;
        WorkerThread& operator=(WorkerThread&&) = delete;
        ~WorkerThread() = default;

        void WorkLoop();
        bool Busy() const { return work != nullptr; }
        void Exit();
        void Join();

        vk::UniqueHandle<vk::CommandPool, vk::DispatchLoaderDynamic> graphics_pool;
        vk::UniqueHandle<vk::CommandPool, vk::DispatchLoaderDynamic> compute_pool;

        vk::UniqueHandle<vk::DescriptorPool, vk::DispatchLoaderDynamic> desc_pool;

        WorkerPool* pool{ nullptr };
        Engine* engine{ nullptr };
    private:
#ifndef SINGLETHREAD
        std::thread thread{ &WorkerThread::WorkLoop, this };
#endif
        std::unique_ptr<WorkItem> work;
        bool active{ true };
    };
}
