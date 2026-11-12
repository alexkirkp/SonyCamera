#include "pch.h"
#include "SonyCamera.h"
#include "FocusSteps.h"
#include "FocusMemory.h"
#include <cmath>
#include "Logger.h"
#include "CameraException.h"
#include "Locker.h"
#include "MutexGuard.h"

#define SONY_GET_NEXT_HANDLE_ENABLE
#define SONY_GET_NEXT_HANDLE_EXTRA_PARAMS

SonyCamera::SonyCamera(Device* device)
    : Camera(device)
{
    LOGTRACE(L"In: SonyCamera::SonyCamera");
    LOGTRACE(L"Out: SonyCamera::SonyCamera");
}

SonyCamera::~SonyCamera()
{
    LOGTRACE(L"In: SonyCamera::~SonyCamera");
    LOGTRACE(L"Out: SonyCamera::~SonyCamera");
}

bool
SonyCamera::Initialize()
{
    LOGTRACE(L"In: SonyCamera::Initialize()");
    bool result = true;
    Message* tx;
    Message* rx;

    if (GetDevice()->NeedsSession())
    {
        tx = new Message(COMMAND_OPEN_SESSION);

        tx->AddParam(0x00000001);

        this->m_device->Send(tx);

        delete tx;
    }

    tx = new Message(COMMAND_GET_STORAGE_IDS);

    rx = m_device->Receive(tx);
    //  Ignore result of this method, a7siii (one sample) seems to return a non-success result
    //    result &= rx->IsSuccess();

    LOGTRACE(L"Get Storage ID's >> %s", rx->Dump().c_str());

    delete tx;
    delete rx;

#ifdef SONY_GET_NEXT_HANDLE_ENABLE
    tx = new Message(COMMAND_SONY_GET_NEXT_HANDLE);

    tx->AddParam(0x00000001);
#ifdef SONY_GET_NEXT_HANDLE_EXTRA_PARAMS
    tx->AddParam(0x00000000);  // Test only sending one param (which is what MTP spec says it should be)
    tx->AddParam(0x00000000);
#endif

    rx = m_device->Receive(tx);
    result &= rx->IsSuccess();

    LOGTRACE(L"Get Next Handle (1) >> %s", rx->Dump().c_str());

    delete tx;
    delete rx;

    tx = new Message(COMMAND_SONY_GET_NEXT_HANDLE);

    tx->AddParam(0x00000002);
#ifdef SONY_GET_NEXT_HANDLE_EXTRA_PARAMS
    tx->AddParam(0x00000000);  // Test only sending one param (which is what MTP spec says it should be)
    tx->AddParam(0x00000000);
#endif

    rx = m_device->Receive(tx);
    result &= rx->IsSuccess();

    LOGTRACE(L"Get Next Handle (2) >> %s", rx->Dump().c_str());

    delete tx;
    delete rx;
#endif

    m_extInfoVersion = SONY_EXT_INFO_VERSION_300;
    rx = GetPropertyList();
    result &= rx->IsSuccess();

    m_supportedProperties = CameraSupportedProperties(rx);
    LOGINFO(L"Ext info version %d accepted; camera lists %u properties and controls", m_extInfoVersion, m_supportedProperties.Size());
    LOGTRACE(L"Get Supported Properties (1) >> %s", m_supportedProperties.AsString().c_str());

    delete rx;

    tx = new Message(COMMAND_SONY_GET_NEXT_HANDLE);

    tx->AddParam(0x00000003);
    tx->AddParam(0x00000000);
    tx->AddParam(0x00000000);

    rx = m_device->Receive(tx);
    result &= rx->IsSuccess();

    LOGTRACE(L"Get Next Handle (3) >> %s", rx->Dump().c_str());

    delete tx;
    delete rx;

    rx = GetPropertyList();
    result &= rx->IsSuccess();

    LOGTRACE(L"Get Supported Properties (2) >> %s", rx->Dump().c_str());

    delete rx;

    try
    {
        RefreshSettings(true);
    }
    catch (CameraException& ex)
    {
        // Failed to refresh settings - assumption is we're not connected to a camera, so return false
        LOGWARN(L"Unable to refresh camera settings: %s", ex.GetMessage().c_str());
        result = false;
    }

    LOGTRACE(L"Out: SonyCamera::Initialize() - result %d", result);

    SetInitialized(result);

    return result;
}

// Sends 0x9202 with m_extInfoVersion. A camera that refuses 300 is asked again with 200, and that
// version is kept for the rest of the session. The caller owns the returned message.
Message*
SonyCamera::GetPropertyList()
{
    std::unique_ptr<Message> tx(new Message(COMMAND_SONY_GET_PROPERTY_LIST));

    tx->AddParam(m_extInfoVersion);

    Message* rx = m_device->Receive(tx.get());

    if (!rx->IsSuccess() && m_extInfoVersion != SONY_EXT_INFO_VERSION_200)
    {
        LOGWARN(L"Camera refused ext info version %d (result x%04x); retrying with %d", m_extInfoVersion, rx->GetCommand(), SONY_EXT_INFO_VERSION_200);
        delete rx;

        m_extInfoVersion = SONY_EXT_INFO_VERSION_200;
        tx.reset(new Message(COMMAND_SONY_GET_PROPERTY_LIST));
        tx->AddParam(m_extInfoVersion);
        rx = m_device->Receive(tx.get());
    }

    return rx;
}

bool
SonyCamera::RefreshSettings(bool refreshFakePropertiesToo)
{
    #ifdef DEBUG
    LOGTRACE(L"In: SonyCamera::RefreshSettings()");
    #endif

    std::unique_ptr<Message> tx(new Message(COMMAND_READ_SETTINGS));
    std::unique_ptr<Message> rx(m_device->Receive(tx.get()));

    if (rx->IsSuccess())
    {
        CameraSettings cs = CameraSettings(rx.get(), m_extInfoVersion >= SONY_EXT_INFO_VERSION_300);

        // Only need to load up the fakes first time thru
        if (!m_settings || refreshFakePropertiesToo)
        {
            LoadFakeProperties(&cs);
        }

        MutexGuard guard(m_hBusyMutex, 1000);

        if (guard)
        {
            if (!m_settings)
            {
                m_settings = new CameraSettings(cs);
            }
            else
            {
                m_settings->Copy(cs);
            }
        }
    }
    else
    {
        throw CameraException(L"Error reading settings from camera");
    }

#ifdef DEBUG
    LOGTRACE(L"Out: SonyCamera::RefreshSettings()");
#endif

    return true;
}

bool
SonyCamera::SetProperty(const Property id, PropertyValue* value)
{
    LOGTRACE(L"In: SonyCamera::SetProperty(x%04x, %s)", (int)id, value->ToString().c_str());

    bool result = false;

    Message* tx;

    // Depending on the property type, we need to send different command
    std::unique_ptr<CameraProperty> prop(GetProperty(id));

    if (prop)
    {
        Locker lock(settingsLock);
        DWORD messageType;
        PropertyInfo* info = prop->GetInfo();

        if (info->GetAccess() == Accessibility::READ_WRITE)
        {
            messageType = COMMAND_SONY_SET_PROPERTY;
        }
        else if (info->GetSonySpare() != 0 && (info->IsButton() || prop->CanNudge()))
        {
            // Button controls and up/down nudges only; a get-only value property is not a button
            messageType = COMMAND_SONY_SET_PROPERTY2;
        }
        else
        {
            throw CameraException(L"Property is not writable");
        }

        tx = new Message((WORD)messageType);

        tx->AddParam((DWORD)id);

        size_t size = value->GetDataSize();

        if (size & 1) {
            size++;
        }

        BYTE* txdata = new BYTE[size];

        memset(txdata, 0, size);

        value->Write(txdata, static_cast<DWORD>(size));
        tx->SetData(txdata, static_cast<DWORD>(size));

        if (m_device->Send(tx))
        {
            result = true;
        }

        delete[] txdata;
        delete tx;
    }

    LOGTRACE(L"Out: SonyCamera::SetProperty(x%08x, %s), - result = %d", (int)id, value->ToString().c_str(), result);

    return result;
}

void
SonyCamera::ResetFocus()
{
    if (m_focusSteps.empty() || m_focusSteps.rbegin()->second == 0)
    {
        LOGERROR(L"ResetFocus: no focus steps configured");
        return;
    }

    DeviceInfo di = GetDeviceInfo();
    UINT16 maxStepSize = m_focusSteps.rbegin()->first;
    UINT16 numResetSteps = 0;
    UINT16 f = 0;

    // If we can assume the user hasn't messed with focus ring manually, we can rely on focus position, speeding reset to infinite
    if (m_focusHandsOff)
    {
        f = m_currentFocusPosition > 0 ? m_currentFocusPosition : 0;
    }

    // The "+1" covers situation where biggest step is not an integer
    numResetSteps = static_cast<UINT16>((GetFocusLimit() - f) / m_focusSteps.rbegin()->second) + 1;

    LOGTRACE(L"Resetting focus, %d moves of size %d to get from %d to %d", numResetSteps, maxStepSize, f, GetFocusLimit());

    for (UINT16 resetPos = 0; resetPos < numResetSteps; resetPos++)
    {
        MoveFocus(maxStepSize);
    }
}

UINT16
SonyCamera::SetFocus(UINT16 focusPosition)
{
    if (m_focusSteps.empty())
    {
        LOGERROR(L"SetFocus: no focus steps configured");
        return GetFocusLimit() + 1;
    }

    // Until this move settles, the position is in doubt: a camera that drops off mid-move must not
    // come back believing the old one
    GetFocusMemory().Forget(GetId());
    m_focusMoveFailed = false;

    if (focusPosition > GetFocusLimit())
    {
        focusPosition = GetFocusLimit();
    }

    DeviceInfo di = GetDeviceInfo();
    UINT16 focusDiff = static_cast<UINT16>(abs(m_lastFocusPosition - focusPosition));

    if (m_currentFocusPosition < 0 || m_currentFocusPosition > GetFocusLimit()
        || di.GetFocusStartMode() == FocusStartMode::RESET_EVERY_TIME
        || (di.GetFocusStartMode() == FocusStartMode::RESET_BIGGER_MOVE && focusDiff > m_lastFocusDiff))
    {
        ResetFocus();
    }

    m_lastFocusPosition = m_currentFocusPosition;
    m_lastFocusDiff = static_cast<UINT16>(abs(m_currentFocusPosition - focusPosition));

    // Plan with the unrounded step sizes: rounding each one (a 2.54 step counted as 2) made the
    // reported position drift from where the lens really was
    FocusPlan plan = PlanFocusMove(m_focusPosition, focusPosition, m_focusSteps, GetFocusLimit());

    if (plan.nudges.empty())
    {
        LOGTRACE(L"Not altering focus, we're as close as we can get @ %.2f vs desired %d", m_focusPosition, focusPosition);
        RememberFocus();

        return (UINT16)m_currentFocusPosition;
    }

    for (INT16 nudge : plan.nudges)
    {
        MoveFocus(nudge);
    }

    RememberFocus();

    return (UINT16)m_currentFocusPosition;
}

void
SonyCamera::RememberFocus()
{
    if (m_focusMoveFailed)
    {
        LOGWARN(L"A focus move failed; not keeping position %.2f for a reconnect", m_focusPosition);
        return;
    }

    GetFocusMemory().Remember(GetId(), m_focusKey, m_focusPosition);
}

UINT16
SonyCamera::GetFocusLimit()
{
    return m_focusLimit;
}

UINT16
SonyCamera::GetFocus()
{
    return m_focusSteps.empty() ? GetFocusLimit() + 1 : (UINT16)m_currentFocusPosition;
}

void
SonyCamera::SetFocusSteps(std::wstring steps, std::wstring sleeps, bool handsOff)
{
    // Steps is expected to have one value for each ascending step size (1..n)
    // Sleeps should have the same if all sleeps are different. If all sleeps are the same, just a single value is needed
    // If no value is present for sleeps, we'll use 100mS * step size
    // Malformed steps leave focus unconfigured (focus calls then return an error) rather than
    // keeping the previous lens's values.
    m_focusHandsOff = handsOff;

    m_focusSteps.clear();
    m_focusSleeps.clear();
    m_focusLimit = 0;
    m_currentFocusPosition = -1;
    m_focusPosition = -1;
    m_focusKey.clear();

    FocusStepConfig cfg;
    std::wstring error;
    bool sleepsDefaulted = false;

    if (!ParseFocusSteps(steps, sleeps, cfg, error, sleepsDefaulted))
    {
        LOGERROR(L"Ignoring malformed focus steps '%s': %s", steps.c_str(), error.c_str());
        return;
    }

    if (sleepsDefaulted)
    {
        LOGERROR(L"Malformed focus sleeps '%s', using default of 100mS * step size", sleeps.c_str());
    }

    m_focusLimit = cfg.limit;
    m_focusSteps = cfg.exactSteps;
    m_focusSleeps = cfg.sleeps;

    for (const auto& st : m_focusSteps)
    {
        LOGINFO(L"Focus step %d = %.2f (of %d) - post-move-sleep = %d", st.first, st.second, m_focusLimit, m_focusSleeps[st.first]);
    }

    // A reconnect makes a new camera object and attaches the lens again. The lens has not moved
    // meanwhile, so pick up where the last one left it instead of reporting an unknown position
    // (which reads as 65535) and resetting.
    m_focusKey = steps + L"|" + sleeps;

    double remembered = -1;

    if (GetFocusMemory().Recall(GetId(), m_focusKey, remembered) && remembered <= m_focusLimit)
    {
        m_focusPosition = remembered;
        m_currentFocusPosition = static_cast<INT16>(std::lround(remembered));

        LOGINFO(L"Focus position %d (%.2f) kept from before this lens was attached again", m_currentFocusPosition, m_focusPosition);
    }
}

void
SonyCamera::MoveFocus(INT16 step)
{
    double diff = m_focusSteps[static_cast<UINT16>(abs(step))];

    LOGTRACE(L"Moving focus by %d (current = %.2f, step_size = %.2f).. ", step, m_focusPosition, diff);

    PropertyValue pv(step);
//    SetProperty(Property::FocusControl, &pv);

    SetPropertyTaskParams params(Property::FocusControl, pv);
    SetPropertyTask task(&params);

    if (!task.Run(this))
    {
        LOGWARN(L"Focus move %d failed", step);
        m_focusMoveFailed = true;
    }

    //if (abs(step) == m_focusSteps.rbegin()->first)
    //{
    //    m_currentFocusPosition = step < 0 ? 0 : GetFocusLimit();
    //}
    //else
    //{
        if (step < 0)
        {
            m_focusPosition -= diff;
        }
        else
        {
            m_focusPosition += diff;
        }

        if (m_focusPosition < 0)
        {
            m_focusPosition = 0;
        }

        if (m_focusPosition > GetFocusLimit())
        {
            m_focusPosition = GetFocusLimit();
        }

        m_currentFocusPosition = static_cast<INT16>(std::lround(m_focusPosition));
//    }

    LOGTRACE(L" new position = %d (%.2f)\n", m_currentFocusPosition, m_focusPosition);

    Sleep(m_focusSleeps[static_cast<UINT16>(abs(step))]);
}