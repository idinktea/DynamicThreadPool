#pragma once
#include <queue>
#include <mutex>
#include <iostream>
using namespace std;

//任务
template <typename T>
struct Task {
	T m_func;
	void* m_arg;
	mutex m_mutex;

	Task(const Task& task) {
		m_func = task.m_func;
		m_arg = task.m_arg;
	}
	Task(T func, void * arg) {
		m_func = func;
		m_arg = arg;
	}
	void * run() {
		lock_guard<mutex> lck(m_mutex);
		m_func(m_arg);
		return m_arg;
	}
};

//任务队列
template <typename T>
class taskQueue {
public:
	taskQueue() = default;
	~taskQueue() = default;
	void taskAdd(Task<T> task);
	void taskAdd(T func, void* arg);
	Task<T> taskGet();
	inline int taskNum();

private:
	queue<Task<T>> m_taskQ;
	mutex m_mutex;
};

template <typename T>
void taskQueue<T>::taskAdd(Task<T> task) 
{
	lock_guard<mutex> lock(m_mutex);
	m_taskQ.push(*task); 
};

template <typename T>
void taskQueue<T>::taskAdd(T func, void* arg) 
{
	lock_guard<mutex> lock(m_mutex);
	Task<T>* task = new Task<T>(func, arg);
	m_taskQ.push(*task);
};

template <typename T>
Task<T> taskQueue<T>::taskGet() {
	lock_guard<mutex> lock(m_mutex);
	Task<T> task = move(m_taskQ.front());
	m_taskQ.pop();
	return task;
}

template <typename T>
inline int taskQueue<T>::taskNum()
{
	lock_guard<mutex> lock(m_mutex);
	return m_taskQ.size(); 
};
