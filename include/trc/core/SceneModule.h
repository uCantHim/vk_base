#pragma once

#include <concepts>

namespace trc
{
    class TaskQueue;

    class SceneModule
    {
    public:
        virtual ~SceneModule() noexcept = default;
        virtual void createTasks(TaskQueue& taskQueue) = 0;
    };

    template<typename T>
    concept SceneModuleT = std::derived_from<T, SceneModule>;
} // namespace trc
