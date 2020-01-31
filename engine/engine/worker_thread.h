#pragma once
#include <thread>
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
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

        vk::UniqueHandle<vk::CommandPool, vk::DispatchLoaderStatic> graphics_pool;
        vk::UniqueHandle<vk::CommandPool, vk::DispatchLoaderStatic> compute_pool;

        vk::UniqueHandle<vk::DescriptorPool, vk::DispatchLoaderStatic> desc_pool;

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
