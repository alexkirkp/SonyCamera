# SonyCamera

`SonyMTPCamera.dll`: a Windows DLL that connects to a compatible Sony DSLR/mirrorless camera over USB
(MTP). Camera settings (properties) can be read and changed, and images can be taken and retrieved.

It was developed primarily for astrophotography. This repository is a personal fork of
[dougforpres/SonyCamera](https://github.com/dougforpres/SonyCamera), which has been dormant since
November 2023; work continues here. More background on the original project: https://retro.kiwi/

## Companion projects

* ASCOM driver: `../ASCOMSonyCameraDriver` (original: https://github.com/dougforpres/ASCOMSonyCameraDriver;
  its wiki has install info)
* NINA plugin: `../NINASonyCameraPlugin`

Both load `SonyMTPCamera.dll`, and the exported API is declared in `SonyMTPCamera/SonyMTPCamera.h`.

## Building

Requires Visual Studio (MSVC v143 toolset, Windows SDK). Open `SonyCamera.sln` and build the
`SonyMTPCamera` project, Release, x86 or x64. From a command line:

    msbuild SonyCamera.sln /t:SonyMTPCamera /p:Configuration=Release /p:Platform=x64

CI (`.github/workflows/msbuild32.yml`) builds Release for both platforms and uploads the DLLs as
artifacts. Other projects in the solution are test and diagnostic tools: `TestDLL` (C#),
`C-TestDLL`, `SonyCameraInfo`, `SonySettingsMonitor` and `LetsGoHunting`. They use the real header
in `SonyMTPCamera/`.

## Dependencies

* [LibRaw](https://www.libraw.org): converts received ARW files to RGGB and RGB
* [libjpeg-turbo](https://www.libjpeg-turbo.org/): converts received JPEG files to RGB
* [libusbK](https://libusbk.sourceforge.net/): USB access; headers and import libraries are bundled in
  `SonyMTPCamera/libusbK/`
