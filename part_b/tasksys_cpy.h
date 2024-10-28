#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <algorithm>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <iostream>
#include <ostream>
#include <set>
#include <chrono>

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

class TaskSystemStateCV {
    public:
        TaskSystemStateCV(int queueSize, int numTotalTasks, IRunnable* runnable, bool inactive, std::vector<TaskID> activeDeps, TaskID taskID, std::mutex* mtx) : m_queueSize(queueSize), m_numTotalTasks(numTotalTasks), m_runnable(runnable), m_completedCount(0), m_inactive(inactive), m_activeDeps(activeDeps), m_taskID(taskID), m_mtx(mtx) {}

	~TaskSystemStateCV() {
	}

	int m_queueSize;
	int m_numTotalTasks;
	IRunnable* m_runnable;
	std::atomic<int> m_completedCount;
	bool m_inactive;
	std::vector<TaskID> m_activeDeps;
	TaskID m_taskID;
	std::mutex* m_mtx;
};


class TaskQueue {
	public:
		TaskQueue(std::vector<TaskSystemStateCV*> tasksReady, std::vector<TaskSystemStateCV*> tasksWaiting, std::condition_variable* notifyWorkersCV, std::condition_variable* notifySignalCV, std::mutex* queueMutex, bool inactive) : m_tasksReady(tasksReady), m_tasksWaiting(tasksWaiting), m_notifyWorkersCV(notifyWorkersCV), m_notifySignalCV(notifySignalCV), m_queueMutex(queueMutex), m_inactive(inactive), m_tasksCreated(0) {}

		std::vector<TaskSystemStateCV*> m_tasksReady;
		std::vector<TaskSystemStateCV*> m_tasksWaiting;
		std::condition_variable* m_notifyWorkersCV;
		std::condition_variable* m_notifySignalCV;
		std::mutex* m_queueMutex;
		bool m_inactive;
		std::set<TaskID> m_tasksCompleted;
		std::atomic<int> m_tasksCreated;
};


/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();

    private:
	int m_numThreads;
	std::thread* m_threads;
	std::atomic<int> m_currTaskID;
	TaskQueue* m_taskQueue;
};

#endif
