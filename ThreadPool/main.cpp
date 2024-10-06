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

std::mutex m;

template <class F>
class Safe_queue
{
	using Task = std::packaged_task<void()>;

public:
	Safe_queue() {};

	Safe_queue(Safe_queue&& safeQueue) : tasks_(std::move(safeQueue.tasks_)), future_queue_{ std::move(safeQueue.future_queue_) } 
	{
		if (&safeQueue == this)
		{
			throw std::invalid_argument("Invalid_argument: this object and the argument object are the same");
		}
	}

	void Submit(F && f)
	{
		std::future<void> fut;

		{
			std::unique_lock<std::mutex> lock(m);//m_);								//mutex???
			fut = tasks_.emplace(std::forward<F>(f)).get_future();
			future_queue_.emplace(std::move(fut));											
		}
		cv_.notify_one();
	}

	void start_task()
	{
		Task task = std::move(this->tasks_.front());
		this->tasks_.pop();
		if (task.valid())
		{
			task();
		}
	}

	bool has_task()
	{
		return !this->tasks_.empty();
	}

	void get_result()
	{
		if (!this->future_queue_.empty())
		{
			auto future = std::move(this->future_queue_.front());
			this->future_queue_.pop();
			future.get();
		}
		
	
	}

private:
	std::queue<std::future<void>> future_queue_;
	std::queue<Task> tasks_;
	//std::mutex m_;
	std::condition_variable cv_;
};


class ThreadPool
{
public:
	ThreadPool(size_t threads_count, Safe_queue<std::function<void()>>&& safe_queue_) : safe_queue{ std::move(safe_queue_) }
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

	void shutdown()
	{
		{
			std::unique_lock<std::mutex> lock(m);
			shutdown_ = true;
		}
		cv.notify_all();
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
	void work()
	{
		while (true)
		{
			
			{
				std::unique_lock<std::mutex> lock(m);
				cv.wait(lock, [this] {
					return safe_queue.has_task() || shutdown_;
					});

				if (safe_queue.has_task())
				{
					safe_queue.start_task();
				}
			}

			if (shutdown_)
			{
			std::unique_lock<std::mutex> lock(m);
				if (!safe_queue.has_task())
				{
					break;
				}
			}
		}
	}

	private:
		std::vector<std::thread> threads_;
		Safe_queue<std::function<void()>> safe_queue;
		//std::mutex m;
		std::condition_variable cv;
		bool shutdown_ = false;
};


//problems: 1)need to move mutex from global parameter, 2) why future start_task without .get()?
int main(int argc, char** argv)
{
	unsigned int cores_quantity = std::thread::hardware_concurrency();
	Safe_queue<std::function<void()>> safe_queue;
	
	for (size_t i = 10; i > 0; --i)
	{
		auto lambda = [i] {//this is a task
			std::cout << i << std::endl;
			//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			};
		std::function<void()> function(lambda);
		safe_queue.Submit(std::move(function));
	}
	
	ThreadPool tp(cores_quantity, std::move(safe_queue));
	
	/*for (size_t i = 0; i < 1; i++)
	{
		safe_queue.get_result();
	}*/
	
	return 0;
}