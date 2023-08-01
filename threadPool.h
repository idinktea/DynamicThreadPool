#pragma once
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <string.h>
#include <iostream>
#include "taskQueue.h"
using namespace std;

constexpr int NUMBER = 2; //管理时创建/销毁1批量的线程数

using callback = void (*)(void*);

template <typename T>
class threadPool 
{
public:
	threadPool(int min = 5, int max = 10);
	threadPool(taskQueue<T>& tasks, int min = 5, int max = 10); //委托构造函数
	~threadPool();
	void manageThread();
	void work(bool isManager);
	void addTask(Task<T> task);
	void addTask(callback func, void * arg);

private:
	//工作线程
	int m_min;			//允许的最小线程数，默认5
	int m_max;			//允许的最大线程数，默认10
	atomic<int> m_num;	//当前实际的线程数
	atomic<int> m_busy;	//忙碌的线程数
	atomic<int> m_exit;	//需销毁的线程数

	thread m_manager;	//管理者线程

	//工作线程容器，环形队列实现，避免销毁一些线程时的移动开销
	thread** m_workerPtrs;
	int m_capacity;

	taskQueue<T> taskQ;

	mutex m_mutex;
	condition_variable cond;

	atomic<bool> stop; //是否销毁线程池
};

//类成员初始化，创建最少工作线程数，创建管理者线程
template <typename T>
threadPool<T>::threadPool(int min, int max) :m_min(min), m_max(max)
{
	//按最少线程个数创建线程
	max = (size_t)max;
	m_workerPtrs = new thread * [max];
	for (int i = 0; i < min; i++) {
		m_workerPtrs[i] = new thread([this]() {this->work(false); });
	}
	m_capacity = min;

	m_manager = thread([this]() {threadPool::manageThread(); });

	m_num = min;
	m_busy = 0;
	m_exit = min;

	stop = false;
};

template <typename T>
threadPool<T>::threadPool(taskQueue<T> & task, int min, int max) :threadPool(min, max)
{
	taskQ = task;
};

template<typename T>
threadPool<T>::~threadPool() 
{
	stop = true;
	m_manager.join();
	//唤醒所有工作线程并阻塞等他们销毁
	cond.notify_all();
	for (int i = 0; i < m_capacity; i++)
		if (m_workerPtrs[i])
			m_workerPtrs[i]->join();
	delete m_workerPtrs;
}

template <typename T>
void threadPool<T>::manageThread() 
{
	cout << "manager " << this_thread::get_id() << " start!" << endl;

	while (!stop) {
		this_thread::sleep_for(chrono::milliseconds(100));

		//销毁多余线程
		//实际线程 > 忙线程*2 且 实际线程 > 最少线程
		unique_lock<mutex> lck(m_mutex);
		if (m_num > m_busy * 2 && m_num > m_min) {
			m_exit = NUMBER;
			for (int i = 0; i < NUMBER && m_num > m_min; i++) {
				cond.notify_one();
			}
		}
		lck.unlock();

		//创建线程
		//线程 < 任务数 且 实际线程 < 最多线程
		lck.lock();
		if (m_num < taskQ.taskNum() && m_num < m_max) {
			int cnt = 0;
			for (int i = 0; i < m_capacity && cnt < NUMBER; i++) {
				//if (memcmp((void *)&m_workers[i], (void*)0, sizeof(m_workers[i])) == 0) {
				if (m_workerPtrs[i] != nullptr)
				{
					cnt++;
					m_num++;
					//m_workerPtrs[i] = new thread(&threadPool::work);
					m_workerPtrs[i] = new thread([this]() {this->work(true); });
				}
			}
			
		}
		lck.unlock();
	}

	cout << "manager " << this_thread::get_id() << " die!" << endl;
}

template <typename T>
void threadPool<T>::work(bool isManager) 
{
	cout << "worker  " << this_thread::get_id() << " created" << (isManager ? " by Manager!" : "!") << endl;

	while (!stop) {
		//任务队列为空就阻塞等
		while (taskQ.taskNum() == 0) 
		{
			{
				unique_lock<mutex> lck(m_mutex);//创建锁并加锁
				cond.wait(lck);//解锁等待资源，有了资源抢到锁后加锁
				if (stop) {
					cout << "worker  " << this_thread::get_id() << " die for stop!" << endl;
					lck.unlock();
					return; //自杀
				}
				if (m_exit > 0) { //是否要自杀
					m_exit--;
					if (m_num > m_min) {
						m_num--;
						for (int i = 0; i < m_capacity; i++) {
							if (this_thread::get_id() == m_workerPtrs[i]->get_id())
								//memset((void *)&m_workers[i], 0, sizeof(m_workers));
								m_workerPtrs[i] == nullptr;
						}
						cout << "worker  " << this_thread::get_id() << " die for Manager!" << endl;
						lck.unlock();
						return; //自杀
					}
				}
			}
		}
		{
			unique_lock<mutex> lock(m_mutex);
			if (stop) {
				cout << "worker  " << this_thread::get_id() << " die for stop!" << endl;
				lock.unlock();
				return; //自杀
			}
		}

		if (taskQ.taskNum() != 0) {
			m_busy++;
			//取一个任务来做
			Task<T> task = taskQ.taskGet();
			void* num = task.run();
			cout << "worker " << this_thread::get_id() << " finished task " << *(int *)num << "!" <<  endl;		
			m_busy--;
		}
	}
	cout << "worker  " << this_thread::get_id() << " die for stop!" << endl;
}

template<typename T>
void threadPool<T>::addTask(Task<T> task)
{
	taskQ.taskAdd(task);
	cond.notify_one();
}

template<typename T>
void threadPool<T>::addTask(callback func, void* arg)
{
	taskQ.taskAdd(func, arg);
	cond.notify_one();
}
