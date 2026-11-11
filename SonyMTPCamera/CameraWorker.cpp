#include "pch.h"
#include "CameraWorker.h"
#include "CameraTask.h"
#include "CameraManager.h"
#include "CameraWorkerFunctions.h"
#include "Logger.h"
#include "CameraException.h"

CameraWorker::CameraWorker(Camera* pCamera)
    : camera(pCamera)
{
    mutexHandle = CreateMutex(nullptr, FALSE, nullptr);
    threadHandle = CreateThread(NULL, 0, &_run, this, 0, &threadId);
}

CameraWorker::~CameraWorker()
{
    stopRequested = true;

    // The worker only checks the flag between steps, and the camera may be in the middle of a long
    // exposure (shouldn't be, but... users!), so wait as long as it takes for the current step
    // (previously this wait happened implicitly on the held mutex).
    WaitForSingleObject(threadHandle, INFINITE);

    // Wait for run to exit
    DWORD exitCode = 0;
    BOOL ret = GetExitCodeThread(threadHandle, &exitCode);

    int waitCount = 0;

    while (exitCode == STILL_ACTIVE && waitCount < WORKER_THREAD_WAIT_FOR_EXIT_RETRIES)
    {
        waitCount++;

        LOGINFO(L"Waiting for camera handler thread to exit %d / %d loops", waitCount, WORKER_THREAD_WAIT_FOR_EXIT_RETRIES);

        Sleep(WORKER_THREAD_WAIT_FOR_EXIT);
        GetExitCodeThread(threadHandle, &exitCode);
    }

    if (exitCode == STILL_ACTIVE)
    {
        LOGERROR(L"Camera handler thread would not exit, terminating with prejudice");
        TerminateThread(threadHandle, -1);
    }

/*    if (currentTask)
    {
        // How do we go about stopping the task?
    }*/

    CloseHandle(mutexHandle);
    CloseHandle(threadHandle);
}

void
CameraWorker::OnCameraStateChange(CameraState newState)
{
    if (newState != lastCameraState)
    {
#ifdef DEBUG
        LOGTRACE(L"Camera State changed from %d to %d", lastCameraState, newState);
#endif
        lastCameraState = newState;
    }
}

DWORD WINAPI
CameraWorker::_run(LPVOID lpParameter)
{
    LOGTRACE(L"In: CameraWorker::_run(x%08x)", lpParameter);

    DWORD result = ((CameraWorker*)lpParameter)->Run();

    LOGTRACE(L"Out: CameraWorker::_run(x%08x) - returning %d", lpParameter, result);

    return result;
}

DWORD CameraWorker::Run()
{
    LOGTRACE(L"In: CameraWorker::Run");

    CameraTask* currentTask = nullptr;
    HANDLE waiter = INVALID_HANDLE_VALUE;
    bool isIdleTask = false;
    int idleCount = 0;

    HRESULT coInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    LOGTRACE(L"CoInitialize returned: %d", coInit);

    while (!stopRequested)
    {
        // The mutex guards the queue only. It is held just long enough to pop a task and is NOT held
        // while a step runs, so QueueTask never blocks behind a long step (e.g. an exposure).
        if (!currentTask)
        {
            WaitForSingleObject(mutexHandle, INFINITE);

            if (!queue.empty())
            {
                std::pair<CameraTask*, HANDLE> next = queue.front();
                queue.pop_front();
                currentTask = next.first;
                waiter = next.second;
#ifdef DEBUG
                LOGTRACE(L"Request pulled from queue");
#endif
            }

            ReleaseMutex(mutexHandle);
        }

        // We can start something
        if (currentTask)
        {
            isIdleTask = false;
            idleCount = 0;
        }
        else
        {
#ifdef DEBUG
            LOGTRACE(L"Nothing to do, queuing up a refresh properties task");
#endif
            currentTask = new RefreshPropertiesTask(false);
            isIdleTask = true;
            idleCount++;

            Sleep(50);
        }

        if (currentTask)
        {
            CameraTaskInfo info;

            info.pCamera = camera;
            info.pTask = currentTask;

            bool complete;

            if (currentTask->SupportsCameraState(lastCameraState))
            {
                complete = currentTask->Step(info);
            }
            else
            {
                // Would never be stepped, leaving its caller blocked: fail it instead
                if (!isIdleTask)
                {
                    LOGWARN(L"Task %s not supported in camera state %d, failing it", currentTask->Name().c_str(), lastCameraState);
                }

                currentTask->Abort();
                idleCount = 1; // never trigger the idle image check for a task that didn't run
                complete = true;
            }

            if (complete)
            {
                if (waiter != INVALID_HANDLE_VALUE)
                {
#ifdef DEBUG
                    LOGTRACE(L"Releasing wait for task %s", currentTask->Name().c_str());
#endif
                    ReleaseSemaphore(waiter, 1, nullptr);
                }
                else
                {
                    // If there is no waiter, there is no way to indicate the task is complete.
                    // It is expected the only task that will fall into this category is the
                    // one auto-generated when there is nothing to do
//                    LOGTRACE(L"Deleting un-waited-for task %s", currentTask->Name().c_str());
                    delete currentTask;
                }

                if (isIdleTask && idleCount % 20 == 0) {
                    // Check to see if there's a photo waiting that we didn't get notified of
                    // This will just queue up a download - when the download completes
#ifdef DEBUG
                    LOGTRACE(L"Checking to see if an image is waiting we haven't been told about");
#endif
                    std::unique_ptr<CameraSettings> cs(info.pCamera->GetSettings());

                    if (isImageReady(cs.get())) {
#ifdef DEBUG
                        LOGINFO(L"There is an image ready, but we didn't get told about it!");
#endif
                        camera->OnImageBufferStatus(ImageBufferStatus::ImageReady);
                    }
                }

                currentTask = nullptr;
                waiter = INVALID_HANDLE_VALUE;
            }
        }

    }

    CoUninitialize();

    LOGTRACE(L"Out: CameraWorker::Run");

    return 0;
}

bool
CameraWorker::QueueTask(CameraTask* task, HANDLE waiter)
{
    // Wait for runner thread to be idle
    DWORD waitResult = WaitForSingleObject(mutexHandle, INFINITE);

    if (waitResult != WAIT_OBJECT_0)
    {
        // Need to error here
    }

    // If there is a waiter, just queue it up - if there is no waiter, look thru
    // queue and only add if there isn't one of same type
    // (QueueAndForget passes INVALID_HANDLE_VALUE, which is what "no waiter" means)
    if (waiter == INVALID_HANDLE_VALUE || waiter == nullptr)
    {
        for (const auto& queued : queue)
        {
            if (queued.second == INVALID_HANDLE_VALUE || queued.second == nullptr)
            {
                if (queued.first->Name() == task->Name())
                {
                    LOGINFO(L"Not queuing task %s as there is already one in queue", task->Name().c_str());
                    task = nullptr;
                    break;
                }
            }
        }
    }

    if (task)
    {
        queue.push_back(std::pair<CameraTask*, HANDLE>(task, waiter));
    }

    // Let runner continue
    ReleaseMutex(mutexHandle);

    return task;
}
