#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <set>
#include <mutex>
#include <future>
#include <thread>
#include <exception>

#include <maya/MMessage.h>

/** That class serializes all core calls to single thread
	it has two important members:
	RunOnceAndWait<T> - this method will execute specified function on that one thread and pause main/calling function until block is complete
	it can also be used to return actual result if needed (bool, MStatus, etc.)
	and KeepRunning - which simulates the stand-alone thread
	so it will keep on executing specified block of code until that block of code returns false

	- for *CPU* rendering I do serialize all the calls and if one call calls function that also calls RunOnceAndWait - that block will execute in the same thread
	- for *GPU* rendering - all *RunOnceAndWait* calls are execute in the calling thread (for maximum speed)

	you might find this macro peppered throughout the code: *RPR_THREAD_ONLY*
	that macro will assert if the function is called from any other thread but RPR thread in *CPU* mode
*/
namespace FireMaya
{

class FireRenderThread
{
	FireRenderThread() = delete;

public:
	struct QueueItemBase
	{
		virtual void Run() = 0;
		virtual bool IsFinished() = 0;
	};
private:
	template<typename T>
	class RunOnceQueueItem : public QueueItemBase
	{
		std::future<T> _result;
		std::promise<T> _promise;
		std::function<T()> _function;
		bool _finished;
	public:
		RunOnceQueueItem(std::function<T()> function) :
			_promise(),
			_function(function),
			_finished(false)
		{
			_result = _promise.get_future();
		}
	public:
		virtual void Run()
		{
			try
			{
				T result = _function();

				_finished = true;
				_promise.set_value(result);
			}
			catch (...)
			{
				_promise.set_exception(std::current_exception());
				_finished = true;
			}
		};

		virtual bool IsFinished()
		{
			return _finished;
		}

		T GetResult()
		{
			return _result.get();
		}
	};

	class RunOnceProcQueueItem : public QueueItemBase
	{
		std::future<void> _result;
		std::promise<void> _promise;
		std::function<void()> _function;
		bool _finished;
	public:
		RunOnceProcQueueItem(std::function<void()> function) :
			_promise(),
			_function(function),
			_finished(false)
		{
			_result = _promise.get_future();
		}
	public:
		virtual void Run()
		{
			try
			{
				_function();

				_finished = true;
				_promise.set_value();
			}
			catch (...)
			{
				_promise.set_exception(std::current_exception());
				_finished = true;
			}
		};

		virtual bool IsFinished()
		{
			return _finished;
		}

		void GetResult()
		{
			return _result.get();
		}
	};

private:
	static std::vector<std::shared_ptr<QueueItemBase>> itemQueue;
	static std::vector<std::shared_ptr<QueueItemBase>> itemQueueForMainThread;
	static std::set<std::thread::id> executingThreadIds;
	static std::mutex itemQueueMutex;
	static std::unique_ptr<std::thread> ptrWorkerThread;
	static std::atomic_bool shouldUseThread;
	static std::atomic_bool runTheThread;
	static MCallbackId callbackId_RPRMainThreadEvent;

public:
	/**
	This method will execute specified function on that one thread and pause main/calling function until block is complete
	it can also be used to return actual result if needed (bool, MStatus, etc.)

	Is case of *UseTheThread* was set to *false* this method will be executed in calling context. This is for use in the
	*GPU* mode, while in *CPU* mode all calls are serialized to this thread.
	*/
	template<typename T>
	static T RunOnceAndWait(std::function<T()> function)
	{
		auto ptr = std::make_shared<RunOnceQueueItem<T>>(function);
		{
			auto shouldPostToQueue = CheckThreadIsRunning();

			if (shouldPostToQueue)
			{
				std::unique_lock<std::mutex> lock(itemQueueMutex);

				itemQueue.emplace(itemQueue.begin(), ptr);
			}
			else
			{
				AutoAddThisExectingThread add;

				return function();
			}
		}

		return AlertWait<T>(ptr);
	}

	static void RunOnceProcAndWait(std::function<void()> function)
	{
		auto ptr = std::make_shared<RunOnceProcQueueItem>(function);
		{
			if (CheckThreadIsRunning())
			{
				std::unique_lock<std::mutex> lock(itemQueueMutex);

				itemQueue.emplace(itemQueue.begin(), ptr);
			}
			else
			{
				AutoAddThisExectingThread add;

				return function();
			}
		}

		AlertWaitProc(ptr);
	}

	template<typename T>
	static T RunOnMainThread(std::function<T()> function)
	{
		auto ptr = std::make_shared<RunOnceQueueItem<T>>(function);
		{
			if (AreWeOnMainThread())
			{
				return function();
			}
			else
			{
				std::unique_lock<std::mutex> lock(itemQueueMutex);

				itemQueueForMainThread.push_back(ptr);
			}
		}

		return ptr->GetResult();
	}

	static void RunProcOnMainThread(std::function<void()> function)
	{
		auto ptr = std::make_shared<RunOnceProcQueueItem>(function);
		{
			if (AreWeOnMainThread())
			{
				return function();
			}
			else
			{
				std::unique_lock<std::mutex> lock(itemQueueMutex);

				itemQueueForMainThread.push_back(ptr);
			}
		}

		return ptr->GetResult();
	}
	/**
	This method which simulates the stand-alone thread, so it will keep on executing specified block of code
	until that block of code returns *false*. Block of code should avoid waiting and sleeping as it shares
	the thread with other callers.
	*/
	static void KeepRunning(std::function<bool()> function);
	/**
	This method will keep on executing specified block of code until that block of code returns *false*.
	Block of code should avoid waiting and sleeping as it shares the main thread.
	*/
	static void KeepRunningOnMainThread(std::function<bool()> function);
	/* Checks if caller is running on RPR Thread */
	static void CheckIsOnRPRThread();
	/* If set to false to just directly run all run and wait calls, returns previous value */
	static bool UseTheThread(bool value);
	/* Call with false to quit the thread */
	static void RunTheThread(bool value);
	/* Checks is thread is still running */
	static bool IsThreadRunning() { return runTheThread; }
	/* Runs items queued to run on the main thread (call only from the main thread) */
	static size_t RunItemsQueuedForTheMainThread();

private:
	struct AutoAddThisExectingThread
	{
		AutoAddThisExectingThread();
		~AutoAddThisExectingThread();
	private:
		bool _added;
	};

private:
	static bool CheckThreadIsRunning();
	static bool AreWeOnMainThread();
	static void ThreadProc(void *);
	static void RPRMainThreadEventCallback(float, float, void *);
	static void RegisterRPREventCallback();
	static void UnregisterRPREventCallback();

	template<typename T>
	static T AlertWait(std::shared_ptr<RunOnceQueueItem<T>> item)
	{
		if (AreWeOnMainThread())
		{
			do
				RunItemsQueuedForTheMainThread();
			while (!item->IsFinished());
		}

		return item->GetResult();
	}

	static void AlertWaitProc(std::shared_ptr<RunOnceProcQueueItem> item)
	{
		if (AreWeOnMainThread())
		{
			do
				RunItemsQueuedForTheMainThread();
			while (!item->IsFinished());
		}

		item->GetResult();
	}
};

extern std::thread::id gMainThreadId; // used for debugging

};

#define RPR_THREAD_ONLY		FireMaya::FireRenderThread::CheckIsOnRPRThread()
#define MAIN_THREAD_ONLY	{ assert(std::this_thread::get_id() == FireMaya::gMainThreadId); }
