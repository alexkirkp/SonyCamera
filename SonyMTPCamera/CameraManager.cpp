#include "pch.h"
#include "CameraManager.h"
#include "SonyCamera.h"
#include "Logger.h"
#include "Registry.h"
#include "CameraDefaults.h"
#include "ShippedDefaults.h"
#include "dll.h"

static CameraManager* cameraManager = nullptr;

CameraManager::CameraManager()
{

}

CameraManager::~CameraManager()
{
    std::unordered_map<HANDLE, Camera*>::iterator it = m_cameraMap.begin();

    while (it != m_cameraMap.end())
    {
        LOGWARN(L"-- deleting camera x%p with name %s", (it->first), (it->second)->GetDeviceInfo().GetManufacturer().c_str());
        (*it).second->Close();
        delete (*it).second;
        it = m_cameraMap.erase(it);
    }
}

void
CameraManager::SetupSupportedDevices()
{
    LOGINFO(L"In: CameraManager::SetupSupportedDevices()");

    // The shipped camera and lens table lives in the Defaults\ layer; the user's own values in
    // Cameras\ and Lenses\ are never written here and take precedence (see Registry).
    registry.Open();

    CameraDefaults table;
    std::string error;

    if (!CameraDefaults::LoadEmbedded(table, error))
    {
        LOGERROR(L"Unable to load the camera defaults table: %S", error.c_str());
    }
    else
    {
        // Before anything reads the registry: old builds' defaults must not act as overrides.
        CameraDefaults legacy;

        if (!CameraDefaults::LoadEmbedded(legacy, error, IDR_LEGACY_DEFAULTS))
        {
            LOGERROR(L"Unable to load the legacy camera defaults table: %S", error.c_str());
        }
        else
        {
            int removed = MigrateLegacyDefaults(registry, legacy);

            if (removed > 0)
            {
                LOGINFO(L"Migrated old camera defaults: removed %d legacy values from the user layer", removed);
            }
        }

        if (SyncShippedDefaults(registry, table))
        {
            LOGINFO(L"Camera defaults table written (version x%08x)", table.Version());
        }
    }

    registry.Close();

    LOGINFO(L"Out: CameraManager::SetupSupportedDevices()");
}

HANDLE
CameraManager::CreateCamera(Device* device, DWORD flags)
{
    LOGTRACE(L"In: CameraManager::CreateCamera(device='%s', flags=x%08x)", device->GetFriendlyName().c_str(), flags);
    Camera* camera = nullptr;
    HANDLE hResult = INVALID_HANDLE_VALUE;

    // Only one create at a time, so two threads can't both make a camera for the same device
    std::lock_guard<std::mutex> createGuard(m_createMutex);

    // See if we already have this device
    {
        CameraLease existing;
        HANDLE existingHandle = INVALID_HANDLE_VALUE;

        {
            std::lock_guard<std::mutex> guard(m_mutex);

            for (auto it = m_cameraMap.begin(); it != m_cameraMap.end(); it++)
            {
                if ((*it).second->GetId() == device->GetId())
                {
                    m_useCount[(*it).second]++;
                    existing = CameraLease(this, (*it).second);
                    existingHandle = (*it).first;
                    break;
                }
            }
        }

        if (existing)
        {
            camera = existing;
            LOGTRACE(L"CameraManager::CreateCamera: Found existing camera with handle x%08x", existingHandle);

            OpenCameraTask task;

            task.Run(camera);

            hResult = CompatibleHandle(task.GetHandle());
        }
    }

    if (hResult == INVALID_HANDLE_VALUE)
    {
        // As long as the camera entry is in the registry, we're good to go
        std::wostringstream builder;

        LOGTRACE(L"CameraManager::CreateCamera: Not an existing camera, looking to see if '%s' is supported", device->GetFriendlyName().c_str());

        if ((flags & OPEN_OVERRIDE) || device->IsSupported())
        {
            // Make a new camera...
            LOGTRACE(L"CameraManager::CreateCamera: Creating new camera object for '%s'", device->GetFriendlyName().c_str());

            camera = new SonyCamera(device);

            OpenCameraTask task;

            // The task succeeds even when the device fails to open (e.g. still unplugged),
            // so check the handle. Registering it anyway put a dead camera under handle
            // xffffffff, which OpenDevice then tried to initialize and hung on.
            if (task.Run(camera) && task.GetHandle() != INVALID_HANDLE_VALUE)
            {
                hResult = AddCamera(task.GetHandle(), camera);
            }
            else
            {
                LOGWARN(L"CameraManager::CreateCamera: Unable to open '%s'", device->GetFriendlyName().c_str());
                delete camera;
                camera = nullptr;
            }
        }
        else
        {
            LOGWARN(L"CameraManager::CreateCamera: Couldn't find what camera to make for '%s'", device->GetFriendlyName().c_str());
        }
    }

    LOGTRACE(L"Out: CameraManager::CreateCamera - returning x%p", hResult);

    return hResult;
}

HANDLE
CameraManager::AddCamera(HANDLE hCamera, Camera* camera)
{
    LOGTRACE(L"In: CameraManager::AddCamera(x%p, x%p)", hCamera, camera);

    if (hCamera == INVALID_HANDLE_VALUE || CompatibleHandle(hCamera) == CompatibleHandle(INVALID_HANDLE_VALUE))
    {
        LOGERROR(L"CameraManager::AddCamera: refusing to add a camera with an invalid handle");

        return INVALID_HANDLE_VALUE;
    }

    HANDLE shortHandle = CompatibleHandle(hCamera);
    // In 64-bit windows we could get a > 32-bit handle.  The ASCOM code is expecting a 32-bit value.
    // According to MSDN, only the bottom 32-bits are valid, so we can trim the top 32-bits
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        m_cameraMap.insert(std::pair<HANDLE, Camera*>(shortHandle, camera));
    }

    LOGTRACE(L"Out: CameraManager::AddCamera(x%p, x%p), added handle x%p", hCamera, camera, shortHandle);

    return shortHandle;
}

HANDLE
CameraManager::CompatibleHandle(HANDLE handle)
{
#if _WIN64
    uint64_t temp = (uint64_t)handle & 0xffffffff;

    return (HANDLE)temp;
#else
    return handle;
#endif
}

void
CameraManager::RemoveCamera(HANDLE hCamera)
{
    hCamera = CompatibleHandle(hCamera);
    LOGTRACE(L"In: CameraManager::RemoveCamera(x%p)", hCamera);

    {
        std::unique_lock<std::mutex> guard(m_mutex);

        auto it = m_cameraMap.find(hCamera);

        if (it != m_cameraMap.end())
        {
            Camera* camera = it->second;

            // No new lease can be taken once it is out of the map; wait for the existing ones
            m_cameraMap.erase(it);
            m_released.wait(guard, [&] { auto u = m_useCount.find(camera); return u == m_useCount.end() || u->second == 0; });
            m_useCount.erase(camera);
        }
    }

    LOGTRACE(L"Out: CameraManager::RemoveCamera(x%p)", hCamera);
}

void
CameraManager::ReleaseCamera(Camera* camera)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    auto it = m_useCount.find(camera);

    if (it != m_useCount.end() && it->second > 0)
    {
        it->second--;
    }

    m_released.notify_all();
}

void
CameraLease::Release()
{
    if (m_manager && m_camera)
    {
        m_manager->ReleaseCamera(m_camera);
    }

    m_manager = nullptr;
    m_camera = nullptr;
}

CameraLease
CameraManager::GetCameraForHandle(HANDLE hCamera)
{
    hCamera = CompatibleHandle(hCamera);

    if (hCamera == CompatibleHandle(INVALID_HANDLE_VALUE))
    {
        return CameraLease();
    }

    std::lock_guard<std::mutex> guard(m_mutex);

    std::unordered_map<HANDLE, Camera*>::iterator it = m_cameraMap.find(hCamera);

    if (it != m_cameraMap.end())
    {
        m_useCount[it->second]++;

        return CameraLease(this, it->second);
    }
    else
    {
        LOGWARN(L"Unable to find camera for handle x%p", hCamera);
        LOGWARN(L"I have %d cameras in my list", m_cameraMap.size());

        for (it = m_cameraMap.begin(); it != m_cameraMap.end(); it++)
        {
            LOGWARN(L"-- got a x%p with name %s", (it->first), (it->second)->GetDeviceInfo().GetManufacturer().c_str());
        }

        return CameraLease();
    }
}

CameraManager*
GetCameraManager()
{
    // First use of the API initialises the DLL (nothing is done in DllMain)
    Init();

    if (cameraManager == nullptr)
    {
        LOGINFO(L"GetCameraManager: First time thru, creating new CameraManager singleton");
        cameraManager = new CameraManager();
    }

    return cameraManager;
}

void
RemoveCameraManager()
{
    if (cameraManager)
    {
        LOGINFO(L"Destroying CameraManager");
        delete cameraManager;
        cameraManager = nullptr;
    }
}
