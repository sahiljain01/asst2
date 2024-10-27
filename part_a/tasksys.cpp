#include "tasksys.h"
#include <thread>
#include <iostream>

IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemSerial::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

void spawnThread(TaskSystemState*  ts) {
	while (ts->m_queueSize > 0) {
		ts->m_queueMutex->lock();
		if (ts->m_queueSize > 0) {
			int taskToRun = --ts->m_queueSize;
			int numTotalTasks = ts->m_numTotalTasks;
			ts->m_queueMutex->unlock();
			ts->m_runnable->runTask(taskToRun, numTotalTasks);
		}
		else {
			ts->m_queueMutex->unlock();
		}
	}
}

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    m_numThreads = num_threads;
} 

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
	std::mutex* mtx = new std::mutex();
	TaskSystemState* taskSystemState = new TaskSystemState(num_total_tasks, num_total_tasks, mtx, runnable, false);	
	int threads_to_spawn = std::min(num_total_tasks, m_numThreads);
	std::thread threads[threads_to_spawn];
	for (int i = 0; i < threads_to_spawn; i++) {
		threads[i] = std::thread(spawnThread, taskSystemState);
	}
	for (int i = 0; i < threads_to_spawn; i++) {
		threads[i].join();
	}
	delete taskSystemState;
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

void spawnThreadAlwaysSpinning(TaskSystemState* ts) {
	while (!ts->m_inactive) {
		ts->m_queueMutex->lock();
		if (ts->m_queueSize > 0) {
			int taskToRun = --ts->m_queueSize;
			// std::cout << "running task: " << taskToRun << "with queue size: " << ts->m_queueSize << std::endl;
			ts->m_queueMutex->unlock();
			ts->m_runnable->runTask(taskToRun, ts->m_numTotalTasks);
			ts->m_completedCount++;
		}
		else {
			ts->m_queueMutex->unlock();
		}
	}
}

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
	m_numThreads = num_threads - 1;
	std::mutex* mtx = new std::mutex();
	m_tss = new TaskSystemState(0, 0, mtx, nullptr, false);
	m_threads = new std::thread[m_numThreads]; 

	for (int i = 0; i < m_numThreads; i++) {
		m_threads[i] = std::thread(spawnThreadAlwaysSpinning, m_tss);
	}
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
	m_tss->m_inactive = true;
	for (int i = 0; i < m_numThreads; i++) {
		m_threads[i].join();
	}
	delete[] m_threads;
	delete m_tss; 
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    m_tss->m_runnable = runnable;
    m_tss->m_numTotalTasks = num_total_tasks;
    m_tss->m_queueSize = num_total_tasks;
    while (m_tss->m_completedCount != num_total_tasks) {
    	    // std::cout << "queue size: " << m_tss->m_queueSize << std::endl;
	    continue;
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    // std::cout << "queue size final: " << m_tss->m_queueSize << std::endl;
    m_tss->m_runnable = nullptr;
    m_tss->m_completedCount.store(0);
    m_tss->m_numTotalTasks = 0;
    m_tss->m_queueSize = 0;
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

void spawnThreadSleeping(TaskSystemStateCV* ts) {
	while (!ts->m_inactive) { 
		std::unique_lock<std::mutex> lk(*ts->m_queueMutex);
		ts->m_notifyWorkersCV->wait(lk);	
		while (ts->m_queueSize > 0) {
			int taskToRun = --ts->m_queueSize;
			lk.unlock();
			ts->m_runnable->runTask(taskToRun, ts->m_numTotalTasks);
			ts->m_completedCount++;
			if (ts->m_completedCount == ts->m_numTotalTasks) {
				ts->m_notifySignalCV->notify_all();
			}
			lk.lock();
		}
		lk.unlock();
	}
}

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) 
{
	m_numThreads = num_threads-1;
	std::mutex* mtx = new std::mutex();
	std::condition_variable* notifyWorkersCV = new std::condition_variable();
	std::condition_variable* notifySignalCV = new std::condition_variable();
	m_tss = new TaskSystemStateCV(0, 0, mtx, nullptr, false, notifyWorkersCV, notifySignalCV);
	m_threads = new std::thread[m_numThreads]; 

	for (int i = 0; i < m_numThreads; i++) {
		m_threads[i] = std::thread(spawnThreadSleeping, m_tss);
	}
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
	// todo: add delete for tss + thread array
	// add destructor to tasksystemstatecv
	// set ts->m_inactive to True and join all of the relevant threads!
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    m_tss->m_runnable = runnable;
    m_tss->m_numTotalTasks = num_total_tasks;
    m_tss->m_queueSize = num_total_tasks;

    int initialTasksToSchedule = std::min(num_total_tasks, m_numThreads);
    std::unique_lock<std::mutex> lk2(*m_tss->m_queueMutex);
    m_tss->m_notifyWorkersCV->notify_all();
    m_tss->m_notifySignalCV->wait(lk2);
    lk2.unlock();

    m_tss->m_completedCount.store(0);
    m_tss->m_numTotalTasks = 0;
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
