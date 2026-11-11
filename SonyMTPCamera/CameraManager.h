#pragma once
#include <list>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include "Camera.h"
#include "Device.h"

constexpr auto OPEN_OVERRIDE = 0x00000001;

class CameraManager;

// Keeps a camera alive while an API call uses it: RemoveCamera() does not return
// until every lease on the camera has been released, so the caller can then delete it.
// Behaves like a Camera* (implicit conversion, ->, bool).
class CameraLease
{
public:
    CameraLease() = default;
    CameraLease(CameraManager* manager, Camera* camera) : m_manager(manager), m_camera(camera) {}
    CameraLease(const CameraLease&) = delete;
    CameraLease& operator=(const CameraLease&) = delete;
    CameraLease(CameraLease&& other) noexcept : m_manager(other.m_manager), m_camera(other.m_camera) { other.m_manager = nullptr; other.m_camera = nullptr; }
    CameraLease& operator=(CameraLease&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_manager = other.m_manager;
            m_camera = other.m_camera;
            other.m_manager = nullptr;
            other.m_camera = nullptr;
        }

        return *this;
    }
    ~CameraLease() { Release(); }

    void Release();

    Camera* get() const { return m_camera; }
    operator Camera*() const { return m_camera; }
    Camera* operator->() const { return m_camera; }

private:
    CameraManager* m_manager = nullptr;
    Camera* m_camera = nullptr;
};

class CameraManager
{
    friend class CameraLease;

public:
    CameraManager();
    ~CameraManager();
    // *created (if given) is set true only when this call made and registered a new camera,
    // false when it returned an already-open one (or failed)
    HANDLE CreateCamera(Device* device, DWORD flags, bool* created = nullptr);
    HANDLE AddCamera(HANDLE hCamera, Camera* camera);
    // Unregisters the camera, then waits for all leases on it to be released.
    // The caller must not hold a lease on it, and may delete it afterwards.
    void RemoveCamera(HANDLE hCamera);
    // The returned lease is empty if there is no such camera
    CameraLease GetCameraForHandle(HANDLE hCamera);

    static void SetupSupportedDevices();

private:
    HANDLE CompatibleHandle(HANDLE handle);
    void ReleaseCamera(Camera* camera);

    std::mutex m_mutex;                          // guards m_cameraMap and m_useCount
    std::condition_variable m_released;
    std::mutex m_createMutex;                    // serialises CreateCamera
    std::unordered_map<HANDLE, Camera*> m_cameraMap;
    std::unordered_map<Camera*, int> m_useCount;
};

CameraManager* GetCameraManager();
void RemoveCameraManager();
