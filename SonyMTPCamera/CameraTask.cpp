#include "pch.h"
#include "CameraTask.h"
#include "CameraWorker.h"
#include "CameraWorkerFunctions.h"
#include "Camera.h"
#include "Logger.h"
#include "CameraException.h"
#include <exception>

class CameraWorker;

bool
CameraTask::Run(Camera* camera)
{
#ifdef DEBUG
    LOGTRACE(L"In: CameraTask::Run(%s)", name.c_str());
#endif
    // Create semaphore, get camera worker, initate and wait for response
    CameraWorker* pWorker = camera->GetWorker();

    waiter = CreateSemaphore(nullptr, 0, 1, nullptr);

    pWorker->QueueTask(this, waiter);

    // Wait for task to complete
    DWORD waitResult = WaitForSingleObject(waiter, INFINITE);

    CloseHandle(waiter);
    waiter = INVALID_HANDLE_VALUE;
#ifdef DEBUG
    LOGTRACE(L"Out: CameraTask::Run(%s)", name.c_str());
#endif
    return this->success;
}

void
CameraTask::Start(Camera* camera)
{
    // Create semaphore, get camera worker, initate and wait for response
    CameraWorker* pWorker = camera->GetWorker();

    // Initially unsignalled: the worker releases it when the task completes, so Cancel/WaitFor
    // really wait for that. (It used to start signalled, so the owner deleted tasks the worker
    // was still running - a crash when the camera sent image-ready events during a new capture.)
    waiter = CreateSemaphore(nullptr, 0, 1, nullptr);
    cancel = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    pWorker->QueueTask(this, waiter);
}

bool
CameraTask::QueueAndForget(Camera* camera)
{
    CameraWorker* pWorker = camera->GetWorker();

    return pWorker->QueueTask(this, INVALID_HANDLE_VALUE);
}


RefreshPropertiesTask::RefreshPropertiesTask(bool refreshFakePropertiesToo)
    : CameraTask(L"RefreshPropertiesTask")
{
    StateStepInfo steps[] = {
        {
            0,
            L"RefreshSettings",
            refreshSettings,
            0,
            0,
            0,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,      // this terminal step is for success
        },
    };

    SetParam1((void *)(refreshFakePropertiesToo ? 1 : 0));
    SetSteps(steps);
}

RefreshDeviceInfoTask::RefreshDeviceInfoTask()
    : CameraTask(L"RefreshDeviceInfoTask")
{
    StateStepInfo steps[] = {
        {
            0,
            L"RefreshDeviceInfo",
            refreshDeviceInfo,
            0,
            0,
            0,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,      // this terminal step is for success
        },
    };

    SetSteps(steps);
}

GetPreviewTask::GetPreviewTask()
    : CameraTask(L"GetPreviewTask")
{
    StateStepInfo steps[] = {
        {
            0,
            L"RefreshSettings",
            refreshSettings,
            1,
            0,
            0,
            0,
            false,
            false,
        },
        {
            1,
            L"GetPreview",
            getPreview,
            0,
            0,
            0,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,
        },
    };

    SetSteps(steps);
}

Image*
GetPreviewTask::GetImage()
{
    return (Image*)GetResult();
}

InitializeCameraTask::InitializeCameraTask()
    : CameraTask(L"InitializeCameraTask")
{
    StateStepInfo steps[] = {
        {
            0,
            L"InitializeCamera",
            initializeCamera,
            0,
            0,
            0,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,
        },
    };

    SetSteps(steps);
}

OpenCameraTask::OpenCameraTask()
    : CameraTask(L"OpenCameraTask")
{
    StateStepInfo steps[] = {
        {
            0,
            L"OpenCamera",
            openCamera,
            0,
            0,
            0,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,
        },
    };

    SetSteps(steps);
    runStates.push_back(CameraState::Null);
}

HANDLE OpenCameraTask::GetHandle()
{
    return (HANDLE)GetResult();
}

CloseCameraTask::CloseCameraTask(HANDLE hCamera)
    : CameraTask(L"CloseCameraTask")
{
    StateStepInfo steps[] = {
        {
            0,
            L"CloseCamera",
            closeCamera,
            0,
            0,
            0,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,
        },
    };

    SetParam1(hCamera);
    SetSteps(steps);
}

bool
CloseCameraTask::Closed()
{
    return GetResult() != nullptr;
}

SetPropertyTaskParams::SetPropertyTaskParams(Property id, PropertyValue& value)
    : id(id)
{
    this->value = PropertyValue(value);
}

SetPropertyTask::SetPropertyTask(SetPropertyTaskParams* params)
    : CameraTask(L"SetPropertyTask")
{
    StateStepInfo steps[] = {
        {
            0,
            L"setProperty",
            setProperty,
            0,
            0,
            0,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,
        },
    };

    SetSteps(steps);

    SetParam1((void*)params);
}

TakePhotoTask::TakePhotoTask(TakePhotoTaskParams* params)
    : CameraTask(L"TakePhotoTask")
{
    // The most complicated task by far
    // Steps: (from the old CaptureThread class)
    // **STEP 1**
    // 1. Check whether auto-focus enabled, if so - we need to wait half-way thru pressing shutter button
    // 2. Get exposure time setting from camera - adjust exposure time to lower of
    //    requested or set
    // NOTE: Cancel prior to this point just exits
    // **STEP 2**
    // 3. Press shutter button half down
    // NOTE: Cancel at this point returns shutter button to up and exits
    // 4. Wait (if auto-focus)
    // 5. Press shutter button full down
    // NOTE: Cancel after this point just aborts the wait (if bulb mode) and continues processing (as camera will barf if we don't pull the image(s))
    // 6. Wait exposure time
    // 7. Release "full down"
    // 8. Release "half down"
    // **STEP 3**
    // 9. Loop, waiting for camera to indicate image(s) ready
    // 10. Read images until camera reports no more to send
    //     1st image is used, rest are saved or discarded based on save setting
    // 11. Process image
    StateStepInfo steps[] = {
        {
            0,
            L"refreshSettings",
            refreshSettings,
            1,
            2,
            2,
            0,
            false,
            false,
        },
        {
            1,
            L"doCapture",
            doCapture,
            0,
            2,
            2,
            0,
            true,
            false,
        },
        {
            2,
            L"abortDoCapture",
            abortDoCapture,   // Reopens the image-ready window; doCapture never got to
            0,
            0,
            -1,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,
        },
    };

    SetSteps(steps);

    SetParam1((void*)params);
}

TakePhotoTaskParams::TakePhotoTaskParams(double exposureTime, OutputMode mode)
    : exposureTime(exposureTime),
    mode(mode)
{
}

DownloadAndProcessImageTask::DownloadAndProcessImageTask(TakePhotoTaskParams* params)
    : CameraTask(L"DownloadAndProcessImageTask")
{
    StateStepInfo steps[] = {
        {
            0,
            L"refreshSettings",
            refreshSettings,
            1,
            3,
            3,
            0,
            false,
            false,
        },
        {
            1,
            L"downloadImage",
            downloadImage,
            2,
            3,
            -1,
            0,
            false,
            false,
        },
        {
            2,
            L"processImage",
            processImage,
            0,
            0,
            -1,
            0,
            true,
            false,
        },
        {
            3,
            L"refreshSettings",
            refreshSettings,
            0,
            0,
            0,
            0,
            true,
            true,
        },
        {
            9999,       // step id
            L"",        // name
            nullptr,    // pointer to handler function
            0,          // step to move to on success
            0,          // step to move to on fail
            0,          // step to move to on cancel
            0,          // number of retries allowed
            true,       // this is a terminal step
            false,
        },
    };

    SetSteps(steps);
    SetParam1((void*)params);
}

DownloadAndProcessImageTask::~DownloadAndProcessImageTask()
{
    Image* image = (Image*)GetResult();

    if (image)
    {
        delete image;
    }
}

Image*
DownloadAndProcessImageTask::GetImage()
{
    return (Image*)GetResult();
}