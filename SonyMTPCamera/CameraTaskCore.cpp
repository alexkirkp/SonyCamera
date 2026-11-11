#include "pch.h"
#include "CameraTask.h"
#include "Logger.h"
#include "CameraException.h"
#include <algorithm>
#include <exception>

// The camera-free part of CameraTask (construction, step machine, accessors), kept apart from the
// parts that queue work on a Camera so the unit tests can build it on its own.

CameraTask::CameraTask(std::wstring name)
    : name(name)
{
#ifdef DEBUG
    LOGTRACE(L"CameraTask::CameraTask(%s)", name.c_str());
#endif
    status.currentStep = CAMERA_TASK_NOT_STARTED;

    runStates.push_back(CameraState::Connected);
}

CameraTask::~CameraTask()
{
#ifdef DEBUG
    LOGTRACE(L"CameraTask::~CameraTask() for %s", name.c_str());
#endif
    if (waiter != INVALID_HANDLE_VALUE)
    {
        CloseHandle(waiter);
    }

    if (cancel != INVALID_HANDLE_VALUE)
    {
        CloseHandle(cancel);
    }
}

void
CameraTask::SetSteps(StateStepInfo steps[])
{
    for (int i = 0; steps[i].handler != nullptr; i++)
    {
        AddStep(steps[i]);
    }
}

bool
CameraTask::AddStep(StateStepInfo info)
{
    m_steps.insert(std::pair<int, StateStepInfo>(info.id, info));

    return true;
}

bool
CameraTask::SupportsCameraState(CameraState state)
{
    return std::find(runStates.begin(), runStates.end(), state) != runStates.end();
}

void
CameraTask::Prepare()
{
    // Initially unsignalled: the worker releases the waiter when the task completes, so
    // Cancel/WaitFor really wait for that.
    if (waiter == INVALID_HANDLE_VALUE)
    {
        waiter = CreateSemaphore(nullptr, 0, 1, nullptr);
    }

    if (cancel == INVALID_HANDLE_VALUE)
    {
        cancel = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    }
}

DWORD
CameraTask::Cancel(DWORD timeout)
{
    if (cancel != INVALID_HANDLE_VALUE)
    {
        SetEvent(cancel);

        return WaitForSingleObject(waiter, timeout);
    }
    else
    {
        return WAIT_OBJECT_0;
    }
}

DWORD
CameraTask::WaitFor(DWORD timeout)
{
    // Wait for task to complete
    return WaitForSingleObject(waiter, timeout);
}

void
CameraTask::SetResult(void* pResult)
{
    this->result = pResult;
}

void*
CameraTask::GetResult()
{
    return result;
}

HANDLE
CameraTask::GetCancelEventHandle() const
{
    return cancel;
}

void
CameraTask::SetParam1(void* param)
{
    param1 = param;
}

void*
CameraTask::GetParam1()
{
    return param1;
}

void
CameraTask::SetParam2(void* param)
{
    param2 = param;
}

void*
CameraTask::GetParam2()
{
    return param2;
}

bool
CameraTask::IsCancelled() const
{
    return cancelled;
}

bool
CameraTask::IsComplete() const
{
    return this->status.currentStep == CAMERA_TASK_COMPLETE;
}

void
CameraTask::Abort()
{
    status.currentStep = CAMERA_TASK_COMPLETE;
    status.currentStepName = L"<aborted>";
    success = false;
}

bool
CameraTask::Step(CameraTaskInfo& info)
{
    if (status.currentStep == CAMERA_TASK_NOT_STARTED) {
        status.currentStep = 0;
    }

    // Reference, not a copy: retryCount/isActive must persist between calls to Step
    StateStepInfo& stepInfo = m_steps[status.currentStep];

    status.currentStepName = stepInfo.name;

    if (stepInfo.handler == nullptr)
    {
        status.currentStep = CAMERA_TASK_COMPLETE;
    }
    else
    {
        if (stepInfo.isActive)
        {
            // Already started, this must be a retry - lets bump the retry counter
            stepInfo.retryCount++;
        }
        else
        {
            stepInfo.isActive = true;
        }

        std::wstring taskName = info.pTask->Name();
        std::wstring stepName = !stepInfo.name.empty() ? stepInfo.name : L"<unknown>";

#ifdef DEBUG
        LOGTRACE(L"Pre: %s.%s", taskName.c_str(), stepName.c_str());
#endif
        // An exception escaping here would terminate the host process; treat it as a failed step
        StateResult stepResult;

        try
        {
            stepResult = (stepInfo.handler)(info);
        }
        catch (CameraException& e)
        {
            LOGERROR(L"Exception in %s.%s: %s", taskName.c_str(), stepName.c_str(), e.GetMessage().c_str());
            stepResult = StateResult::Fail;
        }
        catch (const std::exception& e)
        {
            LOGERROR(L"std::exception in %s.%s: %S", taskName.c_str(), stepName.c_str(), e.what());
            stepResult = StateResult::Fail;
        }
        catch (...)
        {
            LOGERROR(L"Unknown exception in %s.%s", taskName.c_str(), stepName.c_str());
            stepResult = StateResult::Fail;
        }
#ifdef DEBUG
        LOGTRACE(L"Post: %s.%s (stepResult = %d)", info.pTask->Name().c_str(), stepName.c_str(), stepResult);
#endif
        stepInfo.lastResult = stepResult;

        if (!cancelled && cancel != INVALID_HANDLE_VALUE && stepResult != StateResult::Cancel && stepInfo.cancelId >= 0)
        {
            // Test for cancel event
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0)
            {
                stepResult = StateResult::Cancel;
            }
        }

        if (!stepInfo.isTerminal)
        {
            switch (stepResult)
            {
            case StateResult::Success:
                status.currentStep = stepInfo.successId;
                break;

            case StateResult::Fail:
                status.currentStep = stepInfo.failId >= 0 ? stepInfo.failId : stepInfo.successId;
                break;

            case StateResult::Cancel:
                status.currentStep = stepInfo.cancelId >= 0 ? stepInfo.cancelId : stepInfo.failId >= 0 ? stepInfo.failId : stepInfo.successId;
                cancelled = true;
                break;

            case StateResult::Retry:
                if (stepInfo.retryCount >= stepInfo.maxRetries)
                {
                    stepResult = StateResult::Fail;
                    status.currentStep = stepInfo.failId >= 0 ? stepInfo.failId : stepInfo.successId;
                }
                break;
            }

            if (stepResult != StateResult::Retry)
            {
                // Leaving the step: a later visit starts afresh
                stepInfo.isActive = false;
                stepInfo.retryCount = 0;
            }

            status.currentStepName = L"<pending>";
        }
        else
        {
            status.currentStep = CAMERA_TASK_COMPLETE;
            status.currentStepName = L"<complete>";
            success = stepResult == StateResult::Success;
        }
    }

    return status.currentStep == CAMERA_TASK_COMPLETE;
}

std::wstring
CameraTask::Name()
{
    return name;
}

CameraTaskStatus
CameraTask::GetTaskStatus()
{
    return status;
}

void
CameraTask::SetUserStatus(int userStatus)
{
    status.userStatus = userStatus;
}
