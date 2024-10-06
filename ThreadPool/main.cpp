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


template <class F>
class Safe_queue
{
public:
	Safe_queue()
	{};

	Safe_queue(const Safe_queue& safeQueue) : tasks_(safeQueue.tasks_), future_queue_{safeQueue.future_queue_}
	{
		if (&safe_queue_ == this)
		{
			throw std::invalid_argument("Invalid_argument: this object and the argument object are the same");
		}
	}


	//template <class F>
	/*std::future<void>*/void Submit(F && f)
	{
		std::future<void> fut;


		{
			std::unique_lock<std::mutex> lock(m_);
			fut = tasks_.emplace(std::forward<F>(f)).get_future();
			future_queue_.emplace(fut);
		}
		cv_.notify_one();
		//return fut;
		
	}

	void get_result()
	{
		future_queue_.front().get();
	}

private:
	std::queue<std::future<void>> future_queue_;
	std::queue<Task> tasks_;
	std::mutex m_;
	std::condition_variable cv_;
};

class ThreadPool
{
	using Task = std::packaged_task<void()>;

public:
	ThreadPool(size_t threads_count, Safe_queue<std::function<void()>> safe_queue_) : safe_queue{ safe_queue_ }
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

	//template <class F>//template <typename F>
	//std::future<void> Submit(F&& f)
	//{
	//	std::future<void> fut;


	//	{
	//		std::unique_lock<std::mutex> lock(m_);
	//		fut = tasks_.emplace(std::forward<F>(f)).get_future();
	//	}
	//	cv_.notify_one();
	//	return fut;
	//}

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
	void work()//loop()//here is the main idea of pool of tasks
	{
		while (true)
		{
			Task task;



			{
				std::unique_lock<std::mutex> lock(m);
				cv.wait(lock, [this] {
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
			std::unique_lock<std::mutex> lock(m);
				if (tasks_.empty())
				{
					break;
				}
			}
		}
	}

	private:
		std::vector<std::thread> threads_;
		Safe_queue<std::function<void()>> safe_queue;
		//std::queue<Task> tasks_;

		std::mutex m;
		std::condition_variable cv;
		bool shutdown_ = false;
};



//сделать: оператор копирования/перемезщения, конструктор перемещения у queue
int main(int argc, char** argv)//we have 4 threads what stands for number of computer cores and tasks in 
{
	unsigned int cores_quantity = std::thread::hardware_concurrency();
	Safe_queue<std::function<void()>/*std::packaged_task<void()>*/> safe_queue;
	ThreadPool tp(cores_quantity, safe_queue);

	for (size_t i = 1; i > 0 ; --i)
	{
		auto lambda = [i] {//this is a task
			std::cout << i << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds((i % 3) * 100));
			};
		std::function<void()/*int(int)*/> function(lambda);
		safe_queue.Submit(std::move(function));

	}

	for (size_t i = 0; i < 1; i++)
	{
		safe_queue.get_result();
	}

	return 0;
}