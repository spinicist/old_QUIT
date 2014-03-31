//
//  ThreadPool.h
//  DESPOT
//
//  Created by Tobias Wood on 29/07/2013.
//
//

#ifndef THREAD_POOL
#define THREAD_POOL

#include <iostream>
#include <thread>
#include <functional>
#include <vector>

using namespace std;

class ThreadPool {
	private:
		vector<thread> m_pool;
		size_t m_size;
		bool m_run;

	public:
		ThreadPool(const size_t num_threads = thread::hardware_concurrency());
		ThreadPool(ThreadPool &) = delete;
		ThreadPool(ThreadPool &&) = delete;
		
		void resize(const size_t num_threads);
		void for_loop(const function<void(size_t)> f, const size_t start, const size_t stop, const size_t step);
		void for_loop(const function<void(size_t)> f, const size_t stop);
		void stop();
};
#endif