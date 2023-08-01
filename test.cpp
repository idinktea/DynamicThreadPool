#include "taskQueue.h"
#include "threadPool.h"
#include <iostream>
using namespace std;

void f(void * u) {
	cout << "Running task." << endl;
	this_thread::sleep_for(chrono::milliseconds(400));//影响执行任务的速度
}

int main() {
	//创建并启动线程池
	threadPool<callback> pool(1, 10);

	//添加工作
	for (int i = 0; i < 50; i++) {
		cout << "Add task " << i << endl;
		int * num = new int(i);
		pool.addTask(f, num);
		//this_thread::sleep_for(chrono::milliseconds(200));//影响添加任务的速度
	}
	cout << "Add success!" << endl;
	while (1);
	return 0;
}
