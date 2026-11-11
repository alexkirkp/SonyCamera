#include "pch.h"
#include "CameraTask.h"
#include "TestHarness.h"
#include <stdexcept>
#include <vector>

// The step machine is driven here with plain functions standing in for the camera operations.
namespace
{
    std::vector<int> g_visited;     // ids of the steps that ran, in order
    int g_attempts = 0;             // calls made to the retrying step
    int g_succeedOnAttempt = 0;     // the attempt the retrying step succeeds on (0 = never)

    StateResult ok(CameraTaskInfo&)   { g_visited.push_back(1); return StateResult::Success; }
    StateResult bad(CameraTaskInfo&)  { g_visited.push_back(2); return StateResult::Fail; }
    StateResult stop(CameraTaskInfo&) { g_visited.push_back(3); return StateResult::Cancel; }
    StateResult done(CameraTaskInfo&) { g_visited.push_back(4); return StateResult::Success; }
    StateResult fail(CameraTaskInfo&) { g_visited.push_back(5); return StateResult::Fail; }
    StateResult cleanup(CameraTaskInfo&) { g_visited.push_back(6); return StateResult::Success; }

    StateResult retrying(CameraTaskInfo&)
    {
        g_visited.push_back(7);
        g_attempts++;
        return g_attempts == g_succeedOnAttempt ? StateResult::Success : StateResult::Retry;
    }

    StateResult throwsCamera(CameraTaskInfo&) { throw std::runtime_error("boom"); }
    StateResult throwsOther(CameraTaskInfo&)  { throw 42; }

    void reset(int succeedOn = 0)
    {
        g_visited.clear();
        g_attempts = 0;
        g_succeedOnAttempt = succeedOn;
    }

    // A task whose steps are described by the caller: step 0 is `first`, then a terminal 'done'
    // step (id 10, success) and a terminal 'fail' step (id 11). Ids 20+ are for extra steps.
    class TestTask : public CameraTask
    {
    public:
        TestTask(StateHandler first, int successId, int failId, int cancelId, int maxRetries = 0)
            : CameraTask(L"TestTask")
        {
            AddStep({ 0, L"first", first, successId, failId, cancelId, maxRetries, false, false });
            AddStep({ 10, L"done", done, -1, -1, -1, 0, true, true });
            AddStep({ 11, L"fail", fail, -1, -1, -1, 0, true, false });
            AddStep({ 12, L"cleanup", cleanup, 11, 11, 11, 0, false, false });
        }

        // Runs to completion, returning the number of Step calls it took (capped to catch loops)
        int RunAll()
        {
            CameraTaskInfo info = { nullptr, this };
            int calls = 0;

            while (calls < 50)
            {
                calls++;

                if (Step(info))
                {
                    break;
                }
            }

            return calls;
        }

        bool Succeeded() const { return success; }
    };
}

TEST(CameraTask_SuccessFollowsTheSuccessLinkToACompletedTask)
{
    reset();
    TestTask t(ok, 10, 11, -1);

    CHECK(!t.IsComplete());
    CHECK_EQ(t.RunAll(), 2);
    CHECK(t.IsComplete());
    CHECK(t.Succeeded());
    CHECK((g_visited == std::vector<int>{ 1, 4 }));
    CHECK_EQ(t.GetTaskStatus().currentStep, CAMERA_TASK_COMPLETE);
}

TEST(CameraTask_FailFollowsTheFailLink)
{
    reset();
    TestTask t(bad, 10, 11, -1);

    t.RunAll();
    CHECK(t.IsComplete());
    CHECK(!t.Succeeded());
    CHECK((g_visited == std::vector<int>{ 2, 5 }));
}

TEST(CameraTask_FailFallsBackToTheSuccessLinkWhenThereIsNoFailLink)
{
    reset();
    TestTask t(bad, 10, -1, -1);

    t.RunAll();
    CHECK((g_visited == std::vector<int>{ 2, 4 }));
    CHECK(t.Succeeded());
}

TEST(CameraTask_CancelResultFollowsTheCancelLinkAndMarksTheTaskCancelled)
{
    reset();
    TestTask t(stop, 10, 11, 12);

    CHECK(!t.IsCancelled());
    t.RunAll();
    CHECK(t.IsCancelled());
    CHECK((g_visited == std::vector<int>{ 3, 6, 5 }));   // first -> cleanup -> fail
    CHECK(!t.Succeeded());
}

TEST(CameraTask_CancelFallsBackToFailThenSuccess)
{
    reset();
    TestTask toFail(stop, 10, 11, -1);
    toFail.RunAll();
    CHECK((g_visited == std::vector<int>{ 3, 5 }));

    reset();
    TestTask toSuccess(stop, 10, -1, -1);
    toSuccess.RunAll();
    CHECK((g_visited == std::vector<int>{ 3, 4 }));
}

TEST(CameraTask_RetryRepeatsTheStepUntilItSucceeds)
{
    reset(3);
    TestTask t(retrying, 10, 11, -1, 5);

    CHECK_EQ(t.RunAll(), 4);                     // 3 attempts, then the terminal step
    CHECK_EQ(g_attempts, 3);
    CHECK(t.Succeeded());
}

TEST(CameraTask_RetryGivesUpAfterMaxRetries)
{
    reset(0);
    TestTask t(retrying, 10, 11, -1, 2);

    t.RunAll();
    // The first call is the attempt, then 2 retries; the next result takes the fail link
    CHECK_EQ(g_attempts, 3);
    CHECK(!t.Succeeded());
    CHECK((g_visited == std::vector<int>{ 7, 7, 7, 5 }));
}

TEST(CameraTask_ExceptionsInAStepCountAsFailure)
{
    reset();
    TestTask a(throwsCamera, 10, 11, -1);
    a.RunAll();
    CHECK(a.IsComplete());
    CHECK(!a.Succeeded());

    reset();
    TestTask b(throwsOther, 10, 11, -1);
    b.RunAll();
    CHECK(b.IsComplete());
    CHECK(!b.Succeeded());
}

TEST(CameraTask_AbortCompletesAsFailedWithoutRunning)
{
    reset();
    TestTask t(ok, 10, 11, -1);

    t.Abort();
    CHECK(t.IsComplete());
    CHECK(!t.Succeeded());
    CHECK(g_visited.empty());
}

TEST(CameraTask_ReportsItsNameAndRunStates)
{
    TestTask t(ok, 10, 11, -1);

    CHECK(t.Name() == L"TestTask");
    CHECK(t.SupportsCameraState(CameraState::Connected));
    CHECK(!t.SupportsCameraState(CameraState::Disconnected));
}

TEST(CameraTask_StoresItsParametersAndResult)
{
    TestTask t(ok, 10, 11, -1);
    int a = 1, b = 2, r = 3;

    t.SetParam1(&a);
    t.SetParam2(&b);
    t.SetResult(&r);
    CHECK(t.GetParam1() == &a);
    CHECK(t.GetParam2() == &b);
    CHECK(t.GetResult() == &r);

    t.SetUserStatus(7);
    CHECK_EQ(t.GetTaskStatus().userStatus, 7);
}
