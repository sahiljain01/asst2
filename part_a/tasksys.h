#ifndef _TASKSYS_H
#define _TASKSYS_H

#include "itasksys.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

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


class TaskSystemState {
    public:
        TaskSystemState(int queueSize, int numTotalTasks, std::mutex* mtx, IRunnable* runnable, bool inactive): m_queueSize(queueSize), m_numTotalTasks(numTotalTasks), m_queueMutex(mtx), m_runnable(runnable), m_completedCount(0), m_inactive(inactive) {}
	~TaskSystemState() {
		delete m_queueMutex;
	}
	int m_queueSize;
	int m_numTotalTasks;
	std::mutex* m_queueMutex;
	IRunnable* m_runnable;
	std::atomic<int> m_completedCount;
	bool m_inactive;
};


class TaskSystemStateCV {
    public:
        TaskSystemStateCV(int queueSize, int numTotalTasks, std::mutex* mtx, IRunnable* runnable, bool inactive, std::condition_variable* notifyWorkersCV, std::condition_variable* notifySignalCV): m_queueSize(queueSize), m_numTotalTasks(numTotalTasks), m_queueMutex(mtx), m_runnable(runnable), m_completedCount(0), m_inactive(inactive), m_notifyWorkersCV(notifyWorkersCV), m_notifySignalCV(notifySignalCV) {}
	~TaskSystemStateCV() {
		delete m_queueMutex;
	}
	int m_queueSize;
	int m_numTotalTasks;
	std::mutex* m_queueMutex;
	IRunnable* m_runnable;
	std::atomic<int> m_completedCount;
	bool m_inactive;
	std::condition_variable* m_notifyWorkersCV;
	std::condition_variable* m_notifySignalCV;
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

    private:
	int m_numThreads;
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
    private:
	int m_numThreads;
	TaskSystemState* m_tss;
	std::thread* m_threads;
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
	TaskSystemStateCV* m_tss;
	std::thread* m_threads;

};

#endif
