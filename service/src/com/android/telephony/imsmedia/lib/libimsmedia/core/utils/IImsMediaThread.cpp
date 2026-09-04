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

#include <IImsMediaThread.h>
#include <ImsMediaTrace.h>
#include <mediautils/SchedulingPolicyService.h>
#include <pthread.h>
#include <string.h>

#define MAX_THREAD_NAME_LEN 16

extern void setAudioThreadPriority(int threadId);

IImsMediaThread::IImsMediaThread()
{
    mThreadStopped = true;
}

IImsMediaThread::~IImsMediaThread() {}

void* runThread(void* arg)
{
    if (arg == nullptr)
    {
        IMLOGE0("[runThread] invalid argument");
        return nullptr;
    }

    IImsMediaThread* thread = reinterpret_cast<IImsMediaThread*>(arg);
    return thread->run();
}

bool IImsMediaThread::StartThread(const char* name)
{
    IMLOGD1("[StartThread] name:%s", name != nullptr ? name : "(null)");
    ImsMediaMutex::Autolock lock(mThreadMutex);
    mThreadStopped = false;

    pthread_t thread;
    const int threadError = pthread_create(&thread, nullptr, runThread, this);
    if (threadError != 0)
    {
        mThreadStopped = true;
        IMLOGE1("[StartThread] unable to create thread: %s", strerror(threadError));
        return false;
    }

    if (name)
    {
        if (strlen(name) >= MAX_THREAD_NAME_LEN)
        {
            char shortname[MAX_THREAD_NAME_LEN];
            strncpy(shortname, name, MAX_THREAD_NAME_LEN - 1);
            shortname[MAX_THREAD_NAME_LEN - 1] = '\0';
            pthread_setname_np(thread, shortname);
        }
        else
        {
            pthread_setname_np(thread, name);
        }
    }

    const int detachError = pthread_detach(thread);
    if (detachError != 0)
    {
        IMLOGE1("[StartThread] unable to detach thread: %s", strerror(detachError));
    }
    return true;
}

void IImsMediaThread::SetThreadPriority(pid_t pid, pid_t tid, int priority)
{
    const int err =
            android::requestPriority(pid, tid, priority, false /*isForApp*/, true /*asynchronous*/);
    IMLOGD3("[SetThreadPriority] tid:%u, returned:%d. Err: %s", tid, err, strerror(errno));
}

void IImsMediaThread::StopThread()
{
    ImsMediaMutex::Autolock lock(mThreadMutex);
    mThreadStopped = true;
}

bool IImsMediaThread::IsThreadStopped()
{
    ImsMediaMutex::Autolock lock(mThreadMutex);
    return mThreadStopped;
}
