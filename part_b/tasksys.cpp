#include "tasksys.h"


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
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelThreadPoolSpinning in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

void spawnThreadSleeping(TaskQueue* tq) {
	while (!tq->m_inactive) { 
		std::unique_lock<std::mutex> lk(*tq->m_queueMutex);
		if (tq->m_tasksReady.size() == 0) {
			tq->m_notifyWorkersCV->wait(lk);
		}
		if ((tq->m_inactive) || (tq->m_tasksReady.size() == 0)) {
		       lk.unlock();
	       	       continue;
		}	       
		auto ts = tq->m_tasksReady[0];
		int taskToRun = --ts->m_queueSize;
		auto runnable = ts->m_runnable;
		if (taskToRun == 0) {
			tq->m_tasksReady.erase(tq->m_tasksReady.begin());
		}
		lk.unlock();
		runnable->runTask(taskToRun, ts->m_numTotalTasks);
		int completedCnt = ts->m_completedCount.fetch_add(1) + 1;
		lk.lock();
		if (completedCnt == ts->m_numTotalTasks) {
			for (int j = 0; j < static_cast<int>(tq->m_tasksWaiting.size()); j++) {
				auto tsWaiting = tq->m_tasksWaiting[j];
				tsWaiting->m_activeDeps.erase(std::remove(tsWaiting->m_activeDeps.begin(), tsWaiting->m_activeDeps.end(), ts->m_taskID), tsWaiting->m_activeDeps.end());
			}
			auto it = tq->m_tasksWaiting.begin();
			while (it != tq->m_tasksWaiting.end()) {
				auto currentTask = *it;
				if (currentTask->m_activeDeps.size() == 0) {
					tq->m_tasksReady.push_back(currentTask);
					tq->m_notifyWorkersCV->notify_all();
					it = tq->m_tasksWaiting.erase(it);
				}
				else ++it;
			}
			tq->m_tasksCompleted.insert(ts->m_taskID);
		}
		if (tq->m_tasksCreated == tq->m_tasksCompleted.size()) {
			tq->m_notifySignalCV->notify_all();
		}	
		lk.unlock();
	}
        tq->exited_threads++;	
}

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
	m_numThreads = num_threads;
	m_currTaskID = 0;
	std::mutex* mtx = new std::mutex();
	std::condition_variable* notifyWorkersCV = new std::condition_variable();
	std::condition_variable* notifySignalCV = new std::condition_variable();

	std::vector<TaskSystemStateCV*> tasksReady;
	std::vector<TaskSystemStateCV*> tasksWaiting;

	m_taskQueue = new TaskQueue(tasksReady, tasksWaiting, notifyWorkersCV, notifySignalCV, mtx, false); 
	m_threads = new std::thread[m_numThreads]; 

	for (int i = 0; i < m_numThreads; i++) {
		m_threads[i] = std::thread(spawnThreadSleeping, m_taskQueue);
	}
}



TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
	m_taskQueue->m_inactive = true;
	while (m_taskQueue->exited_threads != m_numThreads) {
		m_taskQueue->m_notifyWorkersCV->notify_all();
	}
	for (int i = 0; i < m_numThreads; i++) {
		m_taskQueue->m_notifyWorkersCV->notify_all();
		m_threads[i].join();
	}
	delete[] m_threads;	
}


void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) 
{
	const std::vector<TaskID> deps;
	runAsyncWithDeps(runnable, num_total_tasks, deps);
        sync();	
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {
    TaskID task_id = m_currTaskID++; 
    m_taskQueue->m_tasksCreated++; 
    std::mutex* taskLevelMutex = new std::mutex();
    std::vector<TaskID> depsCpy;

    std::unique_lock<std::mutex> lk2(*m_taskQueue->m_queueMutex);
    for (auto dep: deps) {
	    if (m_taskQueue->m_tasksCompleted.find(dep) == m_taskQueue->m_tasksCompleted.end()) {
		   depsCpy.push_back(dep); 
	    } 
    }
    auto taskTss = new TaskSystemStateCV(num_total_tasks, num_total_tasks, runnable, false, depsCpy, task_id, taskLevelMutex);
    if (depsCpy.size() > 0) {
	    m_taskQueue->m_tasksWaiting.push_back(taskTss);
    }
    else {
	    m_taskQueue->m_tasksReady.push_back(taskTss);
    }
    lk2.unlock(); 
    m_taskQueue->m_notifyWorkersCV->notify_all();
    return task_id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {
    std::unique_lock<std::mutex> lk2(*m_taskQueue->m_queueMutex);
    if (m_taskQueue->m_tasksCreated != m_taskQueue->m_tasksCompleted.size()) {
	    m_taskQueue->m_notifySignalCV->wait(lk2, [&]() { return (m_taskQueue->m_tasksCreated == m_taskQueue->m_tasksCompleted.size()); }) ;
    }
}



