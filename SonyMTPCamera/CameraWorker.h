#pragma once
#include "pch.h"
#include "Camera.h"
#include <queue>
#include <atomic>
#include "CameraState.h"
#include "CameraTask.h"

constexpr auto WORKER_THREAD_WAIT_FOR_EXIT = 1000;
constexpr auto WORKER_THREAD_WAIT_FOR_EXIT_RETRIES = 5;

// An idle worker sleeps until a task is queued or shutdown is requested. If neither happens it wakes
// after this long to refresh properties and check for an image the camera didn't tell us about (the
// camera normally reports both as events, so this is only a safety net).
constexpr auto WORKER_IDLE_POLL_INTERVAL = 1000;

class Camera;

// When camera state changes, an callback is made to the worker thread so it can do its thing.
// Only one worker can be active at a time
class CameraWorker
{
public:
    CameraWorker(Camera* pCamera);
    ~CameraWorker();

    void OnCameraStateChange(CameraState newState);
    // Never blocks on a running step: the task is queued and this returns at once. A caller that
    // waits on its own semaphore still waits until the worker reaches its task (i.e. after the
    // exposure step finishes); the exposure itself is unaffected.
    bool QueueTask(CameraTask* task, HANDLE waiter);

    static DWORD WINAPI _run(LPVOID lpParameter);
    DWORD Run();

protected:

private:
    HANDLE mutexHandle = INVALID_HANDLE_VALUE;
    HANDLE wakeEvent = nullptr; // auto-reset; set when a task is queued or the worker should stop
    HANDLE threadHandle = INVALID_HANDLE_VALUE;
    DWORD threadId = 0;
    Camera* camera = nullptr;
    CameraState lastCameraState = CameraState::Null;
    std::atomic<bool> stopRequested{ false };
    std::list<std::pair<CameraTask*, HANDLE>> queue;
};