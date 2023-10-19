/**
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef IIMS_MEDIA_THREAD
#define IIMS_MEDIA_THREAD

#include <mutex>

#define MAX_EVENTHANDLER_NAME 256

// Thread priority value used with SCH_FIFO scheduling policy to set Real-Time priority.
#define THREAD_PRIORITY_REALTIME 2

/**
 * @class IImsMediaThread
 * @brief Base class of thread
 *        - Child class should implement run() method.
 *        - Call StartThread() method to start thread.
 */
class IImsMediaThread
{
public:
    IImsMediaThread();
    virtual ~IImsMediaThread();
    bool StartThread(const char* name = nullptr);
    static void SetThreadPriority(pid_t pid, pid_t tid, int priority);
    void StopThread();
    bool IsThreadStopped();
    void* runBase();

protected:
    virtual void* run() = 0;

protected:
    std::mutex mThreadMutex;
    bool mThreadStopped;
};

#endif