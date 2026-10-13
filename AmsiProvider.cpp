#include "src/AmsiProvider.h"
#include <shlwapi.h>


#include <algorithm>
#include <vector>
#pragma comment(lib, "shlwapi.lib")




HRESULT AmsiProvider::Scan(
    IAmsiStream* stream,
    AMSI_RESULT* result)
{
    if (!stream || !result)
        return E_INVALIDARG;

    // Default result
    *result = AMSI_RESULT_NOT_DETECTED;

    //
    // ============================================================
    // 1. Get AMSI stream attributes
    // ============================================================
    //

    auto appName =
        GetStringAttribute(
            stream,
            AMSI_ATTRIBUTE_APP_NAME);

    auto contentName =
        GetStringAttribute(
            stream,
            AMSI_ATTRIBUTE_CONTENT_NAME);

    auto contentSize =
        GetFixedSizeAttribute<ULONGLONG>(
            stream,
            AMSI_ATTRIBUTE_CONTENT_SIZE);

    auto sessionId =
        GetFixedSizeAttribute<ULONGLONG>(
            stream,
            AMSI_ATTRIBUTE_SESSION);

    auto contentAddress =
        GetFixedSizeAttribute<PBYTE>(
            stream,
            AMSI_ATTRIBUTE_CONTENT_ADDRESS);


    //
    // ============================================================
    // 2. Debug logging
    // ============================================================
    //

    wchar_t msg[512];

    swprintf_s(
        msg,
        L"[AMSI] Scan(): "
        L"contentAddress=%p, contentSize=%llu, session=%llu\n",
        contentAddress,
        contentSize,
        sessionId);

    OutputDebugStringW(msg);


    //
    // ============================================================
    // 3. Nothing to scan
    // ============================================================
    //

    if (contentSize == 0)
    {
        OutputDebugStringW(
            L"[AMSI] contentSize == 0\n");

        return S_OK;
    }


    //
    // ============================================================
    // 4. Limit dump size
    // ============================================================
    //

    const ULONGLONG MAX_DUMP =
        50ULL * 1024 * 1024;   // 50 MB

    ULONGLONG dumpSize =
        (contentSize < MAX_DUMP)
        ? contentSize
        : MAX_DUMP;


    //
    // ============================================================
    // 5. Try CONTENT_ADDRESS first
    // ============================================================
    //

    std::string content;

    bool usedContentAddress = false;

    if (contentAddress != nullptr)
    {
        OutputDebugStringW(
            L"[AMSI] Trying CONTENT_ADDRESS...\n");

        try
        {
            content.assign(
                reinterpret_cast<const char*>(
                    contentAddress),
                static_cast<size_t>(dumpSize));

            usedContentAddress = true;

            swprintf_s(
                msg,
                L"[AMSI] CONTENT_ADDRESS succeeded. "
                L"Read %llu bytes.\n",
                dumpSize);

            OutputDebugStringW(msg);
        }
        catch (...)
        {
            OutputDebugStringW(
                L"[AMSI] CONTENT_ADDRESS failed.\n");

            content.clear();
            usedContentAddress = false;
        }
    }


    //
    // ============================================================
    // 6. FALLBACK: IAmsiStream::Read()
    // ============================================================
    //

    if (!usedContentAddress)
    {
        OutputDebugStringW(
            L"[AMSI] Falling back to IAmsiStream::Read()...\n");

        content.clear();
        content.reserve(
            static_cast<size_t>(dumpSize));

        const ULONG chunkSize = 64 * 1024;

        std::vector<BYTE> buffer(chunkSize);

        ULONGLONG position = 0;

        HRESULT hr = S_OK;

        while (position < dumpSize)
        {
            ULONG request =
                static_cast<ULONG>(
                    (dumpSize - position < chunkSize)
                    ? (dumpSize - position)
                    : chunkSize);

            ULONG bytesRead = 0;

            hr = stream->Read(
                position,
                request,
                buffer.data(),
                &bytesRead);

            if (FAILED(hr))
            {
                swprintf_s(
                    msg,
                    L"[AMSI] stream->Read() failed: "
                    L"HRESULT=0x%08X\n",
                    static_cast<unsigned int>(hr));

                OutputDebugStringW(msg);

                content.clear();
                break;
            }

            if (bytesRead == 0)
            {
                OutputDebugStringW(
                    L"[AMSI] stream->Read() returned 0 bytes.\n");

                break;
            }

            content.append(
                reinterpret_cast<const char*>(
                    buffer.data()),
                bytesRead);

            position += bytesRead;
        }

        if (!content.empty())
        {
            swprintf_s(
                msg,
                L"[AMSI] IAmsiStream::Read() succeeded. "
                L"Read %zu bytes.\n",
                content.size());

            OutputDebugStringW(msg);
        }
    }


    //
    // ============================================================
    // 7. Dump content if we successfully obtained it
    // ============================================================
    //

    if (!content.empty())
    {
        LogBufferToFile(
            appName,
            contentName,
            sessionId,
            content);

        swprintf_s(
            msg,
            L"[AMSI] Content dumped successfully. "
            L"size=%zu bytes\n",
            content.size());

        OutputDebugStringW(msg);
    }
    else
    {
        OutputDebugStringW(
            L"[AMSI] Could not obtain content.\n");
    }


    //
    // ============================================================
    // 8. Do NOT block the scan
    // ============================================================
    //

    *result = AMSI_RESULT_NOT_DETECTED;

    return S_OK;
}

void AmsiProvider::CloseSession(ULONGLONG /*session*/) { /* nothing */ }

// ==== Đọc buffer từ stream AMSI ====
HRESULT ReadStreamBuffer(IAmsiStream* stream,
    std::string& outContent,
    std::wstring& outAppName,
    std::wstring& outContentName,
    ULONGLONG& outSessionId)
{
    // --- 1. Kích thước content ---
    ULONGLONG contentSize = 0;
    ULONG retSize = 0;
    HRESULT hr = stream->GetAttribute(
        AMSI_ATTRIBUTE_CONTENT_SIZE,
        sizeof(contentSize),
        reinterpret_cast<PBYTE>(&contentSize),
        &retSize);
    if (FAILED(hr))
        return hr;
    if (retSize != sizeof(contentSize))
    {
        return E_FAIL;
    }
    // --- 2. App name (vd: "PowerShell_C:\...\powershell.exe_10.0.x") ---
    ULONG appLen = 0;
    HRESULT hr2 = stream->GetAttribute(AMSI_ATTRIBUTE_APP_NAME, 0, nullptr, &appLen);
   
    if (hr != E_NOT_SUFFICIENT_BUFFER && appLen > 0)
    {
        outAppName.resize(appLen / sizeof(wchar_t));
        stream->GetAttribute(AMSI_ATTRIBUTE_APP_NAME, appLen,
            reinterpret_cast<PBYTE>(outAppName.data()), &appLen);
        // Bỏ null terminator
        while (!outAppName.empty() && outAppName.back() == L'\0')
            outAppName.pop_back();
    }

    // --- 3. Content name (thường là script block ID / filename) ---
    ULONG contentNameLen = 0;
    stream->GetAttribute(AMSI_ATTRIBUTE_CONTENT_NAME, 0, nullptr, &contentNameLen);
    if (contentNameLen > 0)
    {
        outContentName.resize(contentNameLen / sizeof(wchar_t));
        stream->GetAttribute(AMSI_ATTRIBUTE_CONTENT_NAME, contentNameLen,
            reinterpret_cast<PBYTE>(outContentName.data()),
            &contentNameLen);
        while (!outContentName.empty() && outContentName.back() == L'\0')
            outContentName.pop_back();
    }

    // --- 4. Session ID ---
    ULONG sessionSize = 0;
    if (SUCCEEDED(stream->GetAttribute(AMSI_ATTRIBUTE_SESSION,
        sizeof(outSessionId),
        reinterpret_cast<PBYTE>(&outSessionId),
        &sessionSize)))
    {
        // ok
    }

    // --- 5. Đọc nội dung buffer thô ---
    if (contentSize == 0) return S_OK;

    // Giới hạn để tránh buffer khổng lồ (vd 100 MB assembly)
    const ULONGLONG MAX_DUMP = 50ULL * 1024 * 1024; // 50 MB
    ULONGLONG toRead = std::min(contentSize, MAX_DUMP);

    outContent.clear();
    outContent.reserve(static_cast<size_t>(toRead));

    ULONGLONG position = 0;

    const ULONG chunkSize = 64 * 1024;
    std::vector<BYTE> buffer(chunkSize);

    while (position < toRead)
    {
        ULONG request = static_cast<ULONG>(
            std::min<ULONGLONG>(chunkSize, toRead - position ) );

        ULONG bytesRead = 0;

        hr = stream->Read(
            position,
            request,
            buffer.data(),
            &bytesRead
        );

        if (FAILED(hr))
        {
            outContent.clear();
            return hr;
        }

        if (bytesRead == 0)
            break;

        outContent.append(
            reinterpret_cast<const char*>(buffer.data()),
            bytesRead
        );

        position += bytesRead;
    }
  
    return S_OK;
}

static std::atomic<int> g_bufferCounter{ 0 };

// UTF-8 conversion helper
static std::string Narrow(const std::wstring& w)
{
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
        nullptr, 0, nullptr, nullptr);
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
        s.data(), len, nullptr, nullptr);
    return s;
}

static std::string JsonEscape(const std::string& input)
{
    std::string output;
    output.reserve(input.size() + 16);

    for (unsigned char c : input)
    {
        switch (c)
        {
        case '\\':
            output += "\\\\";
            break;

        case '"':
            output += "\\\"";
            break;

        case '\n':
            output += "\\n";
            break;

        case '\r':
            output += "\\r";
            break;

        case '\t':
            output += "\\t";
            break;

        default:
            if (c < 0x20)
            {
                char buf[7];
                sprintf_s(buf, "\\u%04x", c);
                output += buf;
            }
            else
            {
                output += static_cast<char>(c);
            }
        }
    }

    return output;
}

void LogBufferToFile(const std::wstring& appName,
    const std::wstring& contentName,
    ULONGLONG sessionId,
    const std::string& content)
{
    const std::wstring dir = L"C:\\AMSI_Dumps\\";
    CreateDirectoryW(dir.c_str(), nullptr);

    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();

    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    int idx = g_bufferCounter.fetch_add(1);

    // Tên file: epoch_pid_tid_idx.bin
    wchar_t baseName[MAX_PATH];
    swprintf_s(baseName, L"%s%lld_%u_%u_%d",
        dir.c_str(), epoch, pid, tid, idx);

    // --- Ghi buffer thô (.bin) ---
    std::wstring binPath = std::wstring(baseName) + L".bin";
    HANDLE h = CreateFileW(binPath.c_str(), GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;
        WriteFile(h, content.data(), (DWORD)content.size(), &written, nullptr);
        CloseHandle(h);
    }

    // --- Ghi metadata (.json) để dễ grep/filter khi viết YARA rule ---
    std::wstring jsonPath = std::wstring(baseName) + L".json";
    std::string app = JsonEscape(Narrow(appName));
    std::string contentName2 = JsonEscape(Narrow(contentName));
    std::string file = JsonEscape(Narrow(binPath));

    std::ofstream meta(jsonPath);
    meta << "{\n"
        << "  \"timestamp\": " << epoch << ",\n"
        << "  \"app_name\": \"" << app << "\",\n"
        << "  \"content_name\": \"" << contentName2.c_str() << "\",\n"
        << "  \"session_id\": " << sessionId << ",\n"
        << "  \"process_id\": " << pid << ",\n"
        << "  \"thread_id\": " << tid << ",\n"
        << "  \"content_size\": " << content.size() << ",\n"
        << "  \"content_file\": \"" << file << "\"\n"
        << "}\n";
    meta.close();

    // --- Ghi bản text (.txt) nếu buffer là ASCII in được — tiện đọc trực tiếp ---
    bool printable = true;
    for (unsigned char c : content)
    {
        if (c < 0x09 || (c > 0x0D && c < 0x20) || c > 0x7E) { printable = false; break; }
    }
    if (printable && content.size() < 1024 * 1024)
    {
        std::wstring txtPath = std::wstring(baseName) + L".txt";
        std::ofstream txt(txtPath);
        txt.write(content.data(), content.size());
        txt.close();
    }
}


HRESULT AmsiProvider::DisplayName(LPWSTR* displayName)
{
    if (!displayName)
        return E_POINTER;

    *displayName = nullptr;

    const wchar_t* name = L"Sample AMSI Monitoring Provider";

    size_t length = wcslen(name) + 1;
    size_t bytes = length * sizeof(wchar_t);

    *displayName = static_cast<LPWSTR>(
        CoTaskMemAlloc(bytes)
        );

    if (!*displayName)
        return E_OUTOFMEMORY;

    memcpy(*displayName, name, bytes);

    return S_OK;
}



//----------Yara rule generation helper----------
