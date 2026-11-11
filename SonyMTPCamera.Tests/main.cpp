#include "pch.h"
#include "Logger.h"
#include "TestHarness.h"

// The DLL defines these in dllmain.cpp; the sources under test expect them to exist.
Logger logger;
HINSTANCE dllInstance = nullptr;

// String resources live in the DLL; the property parser tests don't need real text.
#include "ResourceLoader.h"
const std::wstring ResourceLoader::GetString(DWORD) { return L""; }

int main()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    int ran = 0;

    for (const auto& test : testing::Registry())
    {
        int before = testing::Failures();

        std::printf("%s\n", test.name);
        test.body();
        ran++;

        if (testing::Failures() != before)
        {
            std::printf("  -> failed\n");
        }
    }

    std::printf("\n%d tests, %d failed checks\n", ran, testing::Failures());

    return testing::Failures() == 0 ? 0 : 1;
}
