// dllmain.cpp
// Entry point của AMSI Provider DLL + COM registration
//
// Build: x64 Release DLL
// Link:  runtimeobject.lib  ole32.lib  oleaut32.lib
//        advapi32.lib       shlwapi.lib
//
// Cài đặt:  regsvr32 AmsiProvider.dll       (elevated)
// Gỡ cài:   regsvr32 /u AmsiProvider.dll    (elevated)

#include "AmsiProvider.h"
#include <shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

using namespace Microsoft::WRL;

// ============================================================
// WRL Class Factory — phải đặt trong .cpp, không phải .h
// Macro này định nghĩa một const object nên chỉ được xuất hiện
// đúng một lần trong toàn bộ translation unit
// ============================================================

CoCreatableClass(AmsiProvider);
static const CLSID CLSID_AmsiProvider = __uuidof(AmsiProvider);
HMODULE g_currentModule = nullptr;
// ============================================================
// CLSID của provider — thay bằng GUID mới khi ship thật
// Tạo bằng: uuidgen / Visual Studio > Tools > Create GUID
// ============================================================
// {2E5D8A62-77F9-4F7B-A90C-2744820139B2}


static const wchar_t* PROVIDER_NAME = L"Sample AMSI Monitoring Provider";
static const wchar_t* THREADING_MODEL = L"Both";      




// ============================================================
// DllMain
// ============================================================
BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID /*reserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_currentModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }

    return TRUE;
}


// ============================================================
// COM Server exports — bắt buộc cho in-proc COM server
// ============================================================
STDAPI DllCanUnloadNow()
{
    return Module<InProc>::GetModule().Terminate()
        ? S_OK
        : S_FALSE;
}

STDAPI DllGetClassObject(_In_  REFCLSID rclsid,
    _In_  REFIID   riid,
    _Out_ LPVOID* ppv)
{
    return Module<InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
}

// ============================================================
// Helper: lấy đường dẫn DLL hiện tại
// ============================================================
static std::wstring GetDllPath()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(g_currentModule, path, ARRAYSIZE(path));
    return path;
}

// ============================================================
// Helper: CLSID → wstring  "{XXXXXXXX-...}"
// ============================================================
static std::wstring ClsidToString(const CLSID& clsid)
{
    wchar_t buf[64] = {};
    StringFromGUID2(clsid, buf, ARRAYSIZE(buf));
    return buf;
}

// ============================================================
// Helper: write registry string value
// ============================================================
static HRESULT RegSetSZ(HKEY hKey, const wchar_t* valueName, const wchar_t* data)
{
    DWORD cbData = static_cast<DWORD>((wcslen(data) + 1) * sizeof(wchar_t));
    LONG  rc = RegSetValueExW(hKey, valueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(data), cbData);
    return HRESULT_FROM_WIN32(rc);
}

// ============================================================
// RegisterProvider
// Đăng ký DLL làm AMSI Provider dưới hai key:
//   1. HKLM\SOFTWARE\Classes\CLSID\{...}\InProcServer32   (COM server)
//   2. HKLM\SOFTWARE\Microsoft\AMSI\Providers\{...}       (AMSI enrollment)

// ============================================================
HRESULT RegisterProvider()
{
    std::wstring clsidStr = ClsidToString(CLSID_AmsiProvider);
    std::wstring dllPath = GetDllPath();
    HRESULT hr = S_OK;
    HKEY    hKey = nullptr;

    // --------------------------------------------------------
    // 1. COM InProcServer32 registration
    // --------------------------------------------------------
    wchar_t comKeyPath[256] = {};
    StringCchPrintfW(comKeyPath, ARRAYSIZE(comKeyPath),
        L"SOFTWARE\\Classes\\CLSID\\%ls\\InProcServer32",
        clsidStr.c_str());

    LONG rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, comKeyPath,
        0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, nullptr, &hKey, nullptr);
    if (rc != ERROR_SUCCESS) return HRESULT_FROM_WIN32(rc);

    hr = RegSetSZ(hKey, nullptr, dllPath.c_str());   // (Default) = dll path
    if (SUCCEEDED(hr))
        hr = RegSetSZ(hKey, L"ThreadingModel", THREADING_MODEL);

    RegCloseKey(hKey);
    if (FAILED(hr)) return hr;

    // ProgID / DisplayName
    wchar_t clsidKeyPath[256] = {};
    StringCchPrintfW(clsidKeyPath, ARRAYSIZE(clsidKeyPath),
        L"SOFTWARE\\Classes\\CLSID\\%ls", clsidStr.c_str());

    rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, clsidKeyPath,
        0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, nullptr, &hKey, nullptr);
    if (rc == ERROR_SUCCESS) {
        RegSetSZ(hKey, nullptr, PROVIDER_NAME);
        RegCloseKey(hKey);
    }

    // --------------------------------------------------------
    // 2. AMSI Provider enrollment
    // --------------------------------------------------------
    wchar_t amsiKeyPath[256] = {};
    StringCchPrintfW(amsiKeyPath, ARRAYSIZE(amsiKeyPath),
        L"SOFTWARE\\Microsoft\\AMSI\\Providers\\%ls",
        clsidStr.c_str());

    rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, amsiKeyPath,
        0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, nullptr, &hKey, nullptr);
    if (rc != ERROR_SUCCESS) return HRESULT_FROM_WIN32(rc);
    RegCloseKey(hKey);

    return S_OK;
}

// ============================================================
// UnregisterProvider
// Xóa tất cả registry keys đã tạo
// ============================================================
HRESULT UnregisterProvider()
{
    std::wstring clsidStr = ClsidToString(CLSID_AmsiProvider);

    // Xóa COM registration
    wchar_t comKeyPath[256] = {};
    StringCchPrintfW(comKeyPath, ARRAYSIZE(comKeyPath),
        L"SOFTWARE\\Classes\\CLSID\\%ls",
        clsidStr.c_str());
    SHDeleteKeyW(HKEY_LOCAL_MACHINE, comKeyPath);

    // Xóa AMSI enrollment
    wchar_t amsiKeyPath[256] = {};
    StringCchPrintfW(amsiKeyPath, ARRAYSIZE(amsiKeyPath),
        L"SOFTWARE\\Microsoft\\AMSI\\Providers\\%ls",
        clsidStr.c_str());
    SHDeleteKeyW(HKEY_LOCAL_MACHINE, amsiKeyPath);

    return S_OK;
}

// ============================================================
// DllRegisterServer / DllUnregisterServer
// Được gọi bởi regsvr32
// ============================================================
STDAPI DllRegisterServer()
{
    return RegisterProvider();
}

STDAPI DllUnregisterServer()
{
    return UnregisterProvider();
}

