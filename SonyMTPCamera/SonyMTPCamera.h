#pragma once
#include <minwindef.h>

constexpr auto IMAGEFLAG_SAVE_RAW_DATA = 0x0001;

constexpr auto INFOFLAG_PASSIVE = 0x0000;
constexpr auto INFOFLAG_ACTIVE = 0x0001;
constexpr auto INFOFLAG_INCLUDE_SETTINGS = 0x0002;

constexpr auto PROPERTYVALUE_TYPE_UINT16 = 0x0004;
constexpr auto PROPERTYVALUE_TYPE_UINT32 = 0x0006;

constexpr auto PROPERTYVALUE_FLAG_WRITABLE = 0x0002;

constexpr auto OPENDEVICEEX_OPEN_ANY_DEVICE = 0x0001;

constexpr auto CAMERAFLAGS_SUPPORTS_LIVEVIEW = 0x00000001;

//#pragma pack(push, 1)
typedef struct
{
    DWORD version;
    DWORD imageWidthPixels;
    DWORD imageHeightPixels;
    DWORD imageWidthCroppedPixels;
    DWORD imageHeightCroppedPixels;
    DWORD bayerXOffset;
    DWORD bayerYOffset;
    DWORD cropMode;
    double exposureTimeMin;
    double exposureTimeMax;
    double exposureTimeStep;
    double pixelWidth;
    double pixelHeight;
    DWORD  bitsPerPixel;

    LPWSTR manufacturer;
    LPWSTR model;
    LPWSTR serialNumber;
    LPWSTR deviceName;
    LPWSTR sensorName;
    LPWSTR deviceVersion;
} DEVICEINFO;

typedef struct
{
    DWORD flags;
    DWORD imageWidthPixels;
    DWORD imageHeightPixels;
    DWORD imageWidthCroppedPixels;
    DWORD imageHeightCroppedPixels;
    DWORD previewWidthPixels;
    DWORD previewHeightPixels;
    DWORD bayerXOffset;
    DWORD bayerYOffset;
    double pixelWidth;
    double pixelHeight;
} CAMERAINFO;

typedef struct
{
    DWORD size;
    BYTE* data;
    DWORD status;
    DWORD imageMode;
    DWORD width;
    DWORD height;
    DWORD flags;
    DWORD metaDataSize;
    BYTE* metaData;
    double duration;
} IMAGEINFO;

typedef struct
{
    DWORD value;
    LPWSTR name;
} PROPERTYVALUEOPTION;

typedef struct
{
    DWORD id;
    DWORD value;
    LPWSTR text;
} PROPERTYVALUE;

typedef struct
{
    DWORD id;
    WORD type;
    WORD flags;
    LPWSTR name;
    DWORD valueCount;
} PROPERTYDESCRIPTOR;

typedef struct
{
    LPWSTR id;
    LPWSTR manufacturer;
    LPWSTR model;
    LPWSTR devicePath;
} PORTABLEDEVICEINFO;

typedef struct
{
    LPWSTR id;
    LPWSTR manufacturer;
    LPWSTR model;
    LPWSTR lensPath;
} LENSINFO;
//#pragma pack(pop)

#ifdef _SONYMTPCAMERA_DLL_BUILD
#define IMPEXP __declspec(dllexport)
#else
#define IMPEXP __declspec(dllimport)
#endif

extern "C" {
    // Functions used by ASCOM Driver plus others
    IMPEXP DWORD   GetDeviceCount();
    IMPEXP HANDLE  OpenDevice(LPWSTR cameraName);
    IMPEXP HANDLE  OpenDeviceEx(LPWSTR cameraName, DWORD flags);
    IMPEXP void    CloseDevice(HANDLE hCamera);
    IMPEXP HRESULT GetDeviceInfo(HANDLE hCamera, DEVICEINFO* info);
    IMPEXP HRESULT GetPreviewImage(HANDLE hCamera, IMAGEINFO* info);
    IMPEXP HRESULT StartCapture(HANDLE hCamera, IMAGEINFO* info);
    IMPEXP HRESULT GetCaptureStatus(HANDLE hCamera, IMAGEINFO* info);
    IMPEXP HRESULT CancelCapture(HANDLE hCamera, IMAGEINFO* info);
    IMPEXP HRESULT GetCameraInfo(HANDLE hCamera, CAMERAINFO* info, DWORD flags);

    /* Sets *removed to TRUE once the camera has been unplugged (noticed idle or mid-exposure), FALSE
       otherwise. It goes back to FALSE when the camera is opened again. Returns ERROR_SUCCESS, or
       ERROR_INVALID_HANDLE / ERROR_INVALID_PARAMETER and leaves *removed untouched. */
    IMPEXP HRESULT IsCameraRemoved(HANDLE hCamera, BOOL* removed);

    /* Closes every open camera, stops the worker threads and releases the library's resources.
       Hosts should call this before FreeLibrary: unloading the DLL does not tear anything down (doing
       so under the loader lock would deadlock), it only leaves the threads to be reclaimed. The next
       API call re-initialises the library. */
    IMPEXP void    ShutdownLibrary();

    // More specific driver functions

    /* These two functions are used to generate a list of devices connected to computer that can be used to help determine identity
       of any new device we wish to support */
    /* Returns count of ALL portable devices currently connected to this computer.  Does not filter the list like GetDeviceCount does */
    IMPEXP DWORD GetPortableDeviceCount();

    /* Returns basic info about the device */
    IMPEXP HRESULT GetPortableDeviceInfo(DWORD offset, PORTABLEDEVICEINFO* pdinfo);

    /* These two functions operate similarly to GetPortableDevice... but only consider known cameras */
    /* Returns count of known camera devices currently connected to this computer.  Does not filter the list like GetDeviceCount does */
    IMPEXP DWORD GetSupportedDeviceCount();

    /* Returns basic info about the device */
    IMPEXP HRESULT GetSupportedDeviceInfo(DWORD offset, PORTABLEDEVICEINFO* pdinfo);

    IMPEXP HRESULT GetPropertyList(HANDLE hCamera, DWORD* list, DWORD* listSize);
    IMPEXP HRESULT GetPropertyDescriptor(HANDLE hCamera, DWORD propertyId, PROPERTYDESCRIPTOR* descriptor);
    IMPEXP HRESULT GetPropertyValueOption(HANDLE hCamera, DWORD propertyId, PROPERTYVALUEOPTION* option, DWORD index);
    IMPEXP HRESULT GetSinglePropertyValue(HANDLE hCamera, DWORD propertyId, PROPERTYVALUE* value);
    IMPEXP HRESULT GetAllPropertyValues(HANDLE hCamera, PROPERTYVALUE* values, DWORD* count);
    IMPEXP HRESULT SetPropertyValue(HANDLE hCamera, DWORD propertyId, DWORD value);
    IMPEXP HRESULT SetExposureTime(HANDLE hCamera, float exposureTime, PROPERTYVALUE* valueOut);

    /* Functions for Focus Control */
    IMPEXP HRESULT SetFocusPosition(HANDLE hCamera, DWORD* position);
    /* GetFocusPosition/GetFocusLimit return a UINT16-range value on success and
       FOCUS_ERROR (0xFFFFFFFF) if the handle is invalid. (Previously they returned
       ERROR_INVALID_HANDLE (6), which collided with a real position.) */
#define FOCUS_ERROR 0xFFFFFFFFUL
    IMPEXP DWORD GetFocusPosition(HANDLE hCamera);
    IMPEXP DWORD GetFocusLimit(HANDLE hCamera);

    IMPEXP DWORD GetLensCount();
    IMPEXP HRESULT GetLensInfo(DWORD offset, LENSINFO* plinfo);
    IMPEXP HRESULT SetAttachedLens(HANDLE hCamera, LPWSTR lensId);

    // General purpose test function
    IMPEXP HRESULT TestFunc(HANDLE hCamera, DWORD value);
}