#include "FireRenderThread.h"

#if _WIN32
#include <windows.h>
#endif
#include <cassert>

#include <maya/MTimerMessage.h>

using namespace std;

namespace FireMaya
{
vector<shared_ptr<FireRenderThread::QueueItemBase>> FireRenderThread::itemQueue;
vector<shared_ptr<FireRenderThread::QueueItemBase>> FireRenderThread::itemQueueForMainThread;
mutex FireRenderThread::itemQueueMutex;
unique_ptr<thread> FireRenderThread::ptrWorkerThread;
atomic_bool FireRenderThread::shouldUseThread { false };
atomic_bool FireRenderThread::runTheThread { true };
set<thread::id> FireRenderThread::executingThreadIds;

MCallbackId FireRenderThread::callbackId_RPRMainThreadEvent = 0;

std::thread::id gMainThreadId;

class QueueItem : public FireRenderThread::QueueItemBase
{
	std::future<void> _result;
	std::promise<void> _promise;
	std::function<bool()> _function;
	bool _isFinished;
public:
	QueueItem(std::function<bool()> function) :
		_promise(),
		_function(function),
		_isFinished(false)
	{
		_result = _promise.get_future();
	}
public:
	virtual void Run()
	{
		try
		{
			_isFinished = !_function();

			if (_isFinished)
				_promise.set_value();
		}
		catch (...)
		{
			_promise.set_exception(current_exception());

			_isFinished = true;
		}
	};

	virtual bool IsFinished()
	{
		return _isFinished;
	}
};

void FireRenderThread::KeepRunning(std::function<bool()> function)
{
	unique_lock<mutex> lock(itemQueueMutex);

	CheckThreadIsRunning();

	itemQueue.push_back(make_shared<QueueItem>(function));
}

/* Should return true if thread is running, if we are on that thread or we should not use the thread */
bool FireRenderThread::CheckThreadIsRunning()
{
	if (!ptrWorkerThread && runTheThread)
		ptrWorkerThread = make_unique<thread>([] {FireRenderThread::ThreadProc(nullptr); });

	if (shouldUseThread && runTheThread)
	{
		auto found = executingThreadIds.find(this_thread::get_id());

		return found == executingThreadIds.end();
	}
	else
	{
		return false;
	}
}

bool FireRenderThread::AreWeOnMainThread()
{
	assert(gMainThreadId != thread::id());	// Must be initialized!

	return this_thread::get_id() == gMainThreadId;
}

size_t FireRenderThread::RunItemsQueuedForTheMainThread()
{
	decltype(itemQueueForMainThread) queue;

	{
		unique_lock<mutex> lock(itemQueueMutex);
		queue = itemQueueForMainThread;
	}

	auto count = queue.size();

	if (count)
	{
		for (auto item : queue)
			item->Run();

		// Now remove complete items:
		{
			unique_lock<mutex> lock(itemQueueMutex);
			decltype(itemQueueForMainThread) newQueue;

			for (auto item : itemQueueForMainThread)
				if (item->IsFinished() == false)
					newQueue.push_back(item);

			itemQueueForMainThread = newQueue;
		}
	}

	return count;
}


#if _WIN32

const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType;		// Must be 0x1000.
	LPCSTR szName;		// Pointer to name (in user address space).
	DWORD dwThreadID;	// Thread ID (-1=caller thread).
	DWORD dwFlags;		// Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
inline void SetThreadName(DWORD dwThreadID, const char* threadName)
{
#ifdef _WIN32
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
	__try {
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
#pragma warning(pop)
#endif
}

#endif // _WIN32

void FireRenderThread::ThreadProc(void *)
{
#if _WIN32
	SetThreadName(GetCurrentThreadId(), "* FireRenderThread *");
#endif
	executingThreadIds.emplace(this_thread::get_id());

	while (runTheThread)
	{
		decltype(itemQueue) queue;

		{
			unique_lock<mutex> lock(itemQueueMutex);
			queue = itemQueue;
		}

		auto count = queue.size();

		if (queue.empty())
		{
			this_thread::sleep_for(10ms);
		}
		else
		{
			for (auto item : queue)
			{
				item->Run();
				this_thread::yield();
			}

			{
				unique_lock<mutex> lock(itemQueueMutex);
				decltype(itemQueue) newQueue;

				for (auto item : itemQueue)
					if (item->IsFinished() == false)
						newQueue.push_back(item);

				itemQueue = newQueue;
			}

			this_thread::yield();
		}
	}

	executingThreadIds.erase(this_thread::get_id());

	ptrWorkerThread.release();
}

void FireRenderThread::KeepRunningOnMainThread(std::function<bool()> function)
{
	unique_lock<mutex> lock(itemQueueMutex);

	CheckThreadIsRunning();

	itemQueueForMainThread.push_back(make_shared<QueueItem>(function));
}

void FireRenderThread::CheckIsOnRPRThread()
{
	if (runTheThread)
	{
		auto differentThread = CheckThreadIsRunning();

		assert(!differentThread);
	}
}

void FireRenderThread::RunTheThread(bool value)
{
	runTheThread = value;

	if (runTheThread)
	{
		CheckThreadIsRunning();

		RegisterRPREventCallback();
	}
	else
	{
		UnregisterRPREventCallback();

		auto ptr = std::move(FireRenderThread::ptrWorkerThread);
		if (ptr)
		ptr->join();
	}
}

void FireRenderThread::RegisterRPREventCallback()
{
	if (!callbackId_RPRMainThreadEvent)
	{
		MStatus status;
		callbackId_RPRMainThreadEvent = MTimerMessage::addTimerCallback(0.01f, FireRenderThread::RPRMainThreadEventCallback, nullptr, &status);
	}
}

void FireRenderThread::UnregisterRPREventCallback()
{
	if (callbackId_RPRMainThreadEvent)
	{
		MStatus status;
		status = MTimerMessage::removeCallback(callbackId_RPRMainThreadEvent);
		callbackId_RPRMainThreadEvent = 0;
	}
}

void FireRenderThread::RPRMainThreadEventCallback(float, float, void *)
{
	if (FireRenderThread::AreWeOnMainThread())
	{
		FireRenderThread::RunItemsQueuedForTheMainThread();
	}
}

bool FireRenderThread::UseTheThread(bool value)
{
	bool prevoius = shouldUseThread;

	/**
	 * Disallow threading. 
	 * Maya documentation (https://knowledge.autodesk.com/search-result/caas/CloudHelp/cloudhelp/2016/ENU/Maya-SDK/files/Technical-Notes-Threading-and-Maya-API-htm.html)
	 * says that it's best to call Maya API function only from main Maya thread.
	 * Forcing this fixes RPRMAYA-24 issue and make plugin to go same path in both CPU and GPU RPR mode.
	 */
	shouldUseThread = false;

	return prevoius;
}

FireRenderThread::AutoAddThisExectingThread::AutoAddThisExectingThread():
	_added(false)
{
	auto id = this_thread::get_id();
	if (executingThreadIds.find(id) == executingThreadIds.end())
	{
		executingThreadIds.emplace(id);
		_added = true;
	}
}

FireRenderThread::AutoAddThisExectingThread::~AutoAddThisExectingThread()
{
	if (_added)
		executingThreadIds.erase(this_thread::get_id());
}

}
