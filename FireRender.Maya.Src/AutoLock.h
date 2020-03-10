/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#pragma once

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
