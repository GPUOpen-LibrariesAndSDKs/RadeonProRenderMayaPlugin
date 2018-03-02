#pragma once

#if MAYA_API_VERSION < 20180000
class MSpinLock;
class MMutexLock;
#endif

namespace RPR
{

	/**
	 * A class that acquires a spin lock in its constructor
	 * and releases it in its destructor. This is useful for
	 * cases where a lock is required and there are multiple
	 * return statements where the lock must be released.
	 * The release will happen automatically when the instance
	 * of this class goes out of scope.
	 */
	template<class Lock>
	class AutoLock
	{
		Lock & m_lock;
	public:
		AutoLock(Lock & lock):
			m_lock(lock)
		{
			m_lock.lock();
		}

		~AutoLock()
		{
			m_lock.unlock();
		}
	};

	/** Auto MSpinLock. */
	typedef AutoLock<MSpinLock> AutoSpinLock;
	/** Auto MMutexLock. */
	typedef AutoLock<MMutexLock> AutoMutexLock;
}
