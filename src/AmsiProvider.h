#pragma once

#define NOMINMAX
#include <windows.h>
#include <amsi.h>
#include <wrl/implements.h>
#include <wrl/module.h>
#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <atomic>
#include <chrono>
#include <algorithm>

using namespace Microsoft::WRL;


HRESULT RegisterProvider();
HRESULT UnregisterProvider();


HRESULT ReadStreamBuffer(
    IAmsiStream* stream,
    std::string& outContent,
    std::wstring& outAppName,
    std::wstring& outContentName,
    ULONGLONG& outSessionId);

void LogBufferToFile(
    const std::wstring& appName,
    const std::wstring& contentName,
    ULONGLONG sessionId,
    const std::string& content);


// Helper template to read a fixed-size attribute from an IAmsiStream
template<typename T>
T GetFixedSizeAttribute(IAmsiStream* stream, AMSI_ATTRIBUTE attr)
{
    T value{};
    ULONG retSize = 0;
    if (stream && SUCCEEDED(stream->GetAttribute(attr, sizeof(T), reinterpret_cast<PBYTE>(&value), &retSize)) && retSize == sizeof(T))
    {
        return value;
    }
    return T{};
}

// Helper to get a string attribute from an IAmsiStream
inline std::wstring GetStringAttribute(IAmsiStream* stream, AMSI_ATTRIBUTE attr)
{
    if (!stream) return L"";
    ULONG len = 0;
    HRESULT hr = stream->GetAttribute(attr, 0, nullptr, &len);
    if (FAILED(hr) || len == 0) return L"";
    std::wstring result(len / sizeof(wchar_t), L'\0');
    hr = stream->GetAttribute(attr, len, reinterpret_cast<PBYTE>(&result[0]), &len);
    if (FAILED(hr)) return L"";
    // Remove any trailing nulls
    while (!result.empty() && result.back() == L'\0') result.pop_back();
    return result;
}

// ============================================================
// AmsiProvider — implements IAntimalwareProvider
// ============================================================
class __declspec(uuid("2E5D8A62-77F9-4F7B-A90C-2744820139B2"))
    AmsiProvider
    : public RuntimeClass<
    RuntimeClassFlags<ClassicCom>,
    IAntimalwareProvider,
    FtmBase>
{
public:
    IFACEMETHOD(DisplayName)(
        _Outptr_ LPWSTR* displayName) override;

    IFACEMETHOD(Scan)(
        _In_ IAmsiStream* stream,
        _Out_ AMSI_RESULT* result) override;

    IFACEMETHOD_(void, CloseSession)(
        _In_ ULONGLONG session) override;
};
