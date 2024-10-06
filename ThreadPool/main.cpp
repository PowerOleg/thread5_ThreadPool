#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <chrono>
#include <future>
#include <utility>
#include <condition_variable>
#include <exception>
#include <stdexcept>

class ThreadPool
{
	using Task = std::packaged_task<void()>;

public:
	ThreadPool(size_t threads_count)
	{
		threads_.reserve(threads_count);
		for (size_t i = 0; i < threads_count; i++)
		{
			threads_.emplace_back(&ThreadPool::work, this);//second argument?
		}
	}

	~ThreadPool()
	{
		join();
	}

	template <class F>//template <typename F>
	std::future<void> Submit(F&& f)
	{
		std::future<void> fut;


		{
			std::unique_lock<std::mutex> lock(m_);
			fut = tasks_.emplace(std::forward<F>(f)).get_future();
		}
		cv_.notify_one();
		return fut;
	}

	void shutdown()
	{
		{
			std::unique_lock<std::mutex> lock(m_);
			shutdown_ = true;
		}
		cv_.notify_all();
	}


private:
	void join()
	{
		shutdown();
		for (auto& t : threads_)
		{
			t.join();
		}
	}

private:
	void work()//loop()//here is the main idea of pool of tasks
	{
		while (true)
		{
			Task task;



			{
				std::unique_lock<std::mutex> lock(m_);
				cv_.wait(lock, [this] {//if predicate is false then this_thread sleeps
					return !tasks_.empty() || shutdown_;
					});

				if (!tasks_.empty())
				{
					task = std::move(tasks_.front());
					tasks_.pop();
				}
			}

			if (task.valid())
			{
				task();
			}


			if (shutdown_)
			{
			std::unique_lock<std::mutex> lock(m_);
				if (tasks_.empty())
				{
					break;
				}
			}
		}
	}

	private:
		std::vector<std::thread> threads_;
		std::queue<Task> tasks_;

		std::mutex m_;
		std::condition_variable cv_;
		bool shutdown_ = false;
};




int main(int argc, char** argv)//we have 4 threads what stands for number of computer cores and tasks in 
{
	unsigned int cores_quantity = std::thread::hardware_concurrency();
	ThreadPool tp(cores_quantity);

	std::vector<std::future<void>> futures;

	for (size_t i = 1; i > 0 ; --i)
	{
		futures.push_back(tp.Submit(
			[i] {//this is a task which is a function to do
				std::cout << i << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds((i % 3) * 100));
			}
		
		
		));
	}

	for (auto& f : futures)//here we get result from tasks
	{
		f.get();
	}

	return 0;
}