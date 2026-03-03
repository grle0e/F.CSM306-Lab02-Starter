#include "tasksys.h"
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>

IRunnable::~IRunnable() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char *TaskSystemSerial::name()
{
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads) : ITaskSystem(num_threads)
{
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable *runnable, int num_total_tasks)
{
    for (int i = 0; i < num_total_tasks; i++)
    {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                          const std::vector<TaskID> &deps)
{
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync()
{
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelSpawn::name()
{
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads) : ITaskSystem(num_threads)
{
    //
    // TODO: CSM306 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable *runnable, int num_total_tasks)
{

    //
    // TODO: CSM306 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    std::vector<std::thread> threads;

    int chunk = (num_total_tasks + num_threads - 1) / num_threads;

    for (int t = 0; t < num_threads; t++)
    {
        int start = t * chunk;
        int end = std::min(start + chunk, num_total_tasks);

        if (start >= end)
            break;

        threads.emplace_back([runnable, start, end, num_total_tasks]() {
            for (int i = start; i < end; i++)
            {
                runnable->runTask(i, num_total_tasks);
            }
        });
    }

    for (auto &th : threads)
    {
        th.join();
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                 const std::vector<TaskID> &deps)
{
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync()
{
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSpinning::name()
{
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads)
    : ITaskSystem(num_threads)
{
    //
    // TODO: CSM306 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    stop = false;
    nextTask = 0;
    totalTasks = 0;
    runnable = nullptr;

    for (int i = 0; i < num_threads; i++)
    {
        workers.emplace_back([this]() {
            while (!stop)
            {
                int task = -1;

                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (nextTask < totalTasks)
                        task = nextTask++;
                }

                if (task != -1)
                {
                    runnable->runTask(task, totalTasks);
                }
            }
        });
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning()
{
    stop = true;
    for (auto &t : workers)
    {
        t.join();
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable *runnable, int num_total_tasks)
{

    //
    // TODO: CSM306 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    {
        std::lock_guard<std::mutex> lock(mtx);
        this->runnable = runnable;
        totalTasks = num_total_tasks;
        nextTask = 0;
    }

    while (true)
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (nextTask >= totalTasks)
            break;
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps)
{
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync()
{
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char *TaskSystemParallelThreadPoolSleeping::name()
{
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads)
    : ITaskSystem(num_threads)
{
    //
    // TODO: CSM306 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    stop = false;
    hasWork = false;
    nextTask = 0;
    totalTasks = 0;
    activeWorkers = 0;
    runnable = nullptr;

    for (int i = 0; i < num_threads; i++)
    {
        workers.emplace_back([this]() {
            while (true)
            {
                std::unique_lock<std::mutex> lock(mtx);

                cv.wait(lock, [this]() { return hasWork || stop; });

                if (stop)
                    return;

                while (nextTask < totalTasks)
                {
                    int task = nextTask++;
                    lock.unlock();
                    runnable->runTask(task, totalTasks);
                    lock.lock();
                }

                activeWorkers--;

                if (activeWorkers == 0)
                {
                    done_cv.notify_one();
                }

                hasWork = false;
            }
        });
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping()
{
    //
    // TODO: CSM306 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    {
        std::lock_guard<std::mutex> lock(mtx);
        stop = true;
        cv.notify_all();
    }

    for (auto &t : workers)
    {
        t.join();
    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable *runnable, int num_total_tasks)
{

    //
    // TODO: CSM306 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    std::unique_lock<std::mutex> lock(mtx);

    this->runnable = runnable;
    totalTasks = num_total_tasks;
    nextTask = 0;
    activeWorkers = num_threads;
    hasWork = true;

    cv.notify_all();

    done_cv.wait(lock, [this]() { return activeWorkers == 0; });
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable *runnable, int num_total_tasks,
                                                              const std::vector<TaskID> &deps)
{

    //
    // TODO: CSM306 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync()
{

    //
    // TODO: CSM306 students will modify the implementation of this method in Part B.
    //

    return;
}
