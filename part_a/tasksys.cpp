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
		int qSize = ts->m_queueSize;
		auto runnable = ts->m_runnable;
		if ((qSize > 0)  && (runnable != nullptr)) {
			int taskToRun = --ts->m_queueSize;
			ts->m_queueMutex->unlock();
			runnable->runTask(taskToRun, ts->m_numTotalTasks);
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
    std::unique_lock<std::mutex> lk(*m_tss->m_queueMutex);
    m_tss->m_runnable = runnable;
    m_tss->m_numTotalTasks = num_total_tasks;
    m_tss->m_queueSize = num_total_tasks;
    m_tss->m_completedCount.store(0);
    lk.unlock();
    while (m_tss->m_completedCount < num_total_tasks) {
    	    // std::cout << "queue size: " << m_tss->m_queueSize << std::endl;
	    continue;
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    // std::cout << "queue size final: " << m_tss->m_queueSize << std::endl;
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
		int qSize = ts->m_queueSize;
		auto runnable = ts->m_runnable;
		int taskJustFinished = -1;
		while ((qSize > 0) && (runnable != nullptr)) {
			int taskToRun = --ts->m_queueSize;
			lk.unlock();
			runnable->runTask(taskToRun, ts->m_numTotalTasks);
			taskJustFinished = ts->m_completedCount.fetch_add(1) + 1;
			lk.lock();
			qSize = ts->m_queueSize;
			runnable = ts->m_runnable;
		}
		lk.unlock();
		if (taskJustFinished == ts->m_numTotalTasks) {
			std::unique_lock<std::mutex> finishedLk(*ts->m_finishedMutex);
			finishedLk.unlock();
			ts->m_notifySignalCV->notify_all();
		 }
		if (ts->m_inactive) {
			break;
		}
	}
}

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) 
{
	m_numThreads = num_threads-1;
	std::mutex* mtx = new std::mutex();
	std::mutex* finishedMtx = new std::mutex();
	std::condition_variable* notifyWorkersCV = new std::condition_variable();
	std::condition_variable* notifySignalCV = new std::condition_variable();
	m_tss = new TaskSystemStateCV(0, 0, mtx, nullptr, false, notifyWorkersCV, notifySignalCV, finishedMtx);
	m_threads = new std::thread[m_numThreads]; 

	for (int i = 0; i < m_numThreads; i++) {
		m_threads[i] = std::thread(spawnThreadSleeping, m_tss);
	}
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() 
{
	m_tss->m_inactive = true;
	m_tss->m_notifyWorkersCV->notify_all();
	for (int i = 0; i < m_numThreads; i++) {
		m_threads[i].join();
	}
	delete[] m_threads;
	delete m_tss; 
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {
    std::unique_lock<std::mutex> lk(*m_tss->m_queueMutex);
    std::unique_lock<std::mutex> finishedLk(*m_tss->m_finishedMutex);
    m_tss->m_runnable = runnable;
    m_tss->m_numTotalTasks = num_total_tasks;
    m_tss->m_queueSize = num_total_tasks;
    m_tss->m_completedCount.store(0);
    lk.unlock();
    m_tss->m_notifyWorkersCV->notify_all();
    // std::unique_lock<std::mutex> lk2(*m_tss->m_queueMutex);
    m_tss->m_notifySignalCV->wait(finishedLk);
    finishedLk.unlock();
    // while (m_tss->m_completedCount < num_total_tasks) {
    	    // std::cout << "queue size: " << m_tss->m_queueSize << std::endl;
//	    continue;
 //   }
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
