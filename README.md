# AMSI Provider — C++ Implementation

## Cấu trúc file

```
AmsiProvider/
├── src/
│   ├── AmsiProvider.h      # Interface + Logger + AmsiProvider class
│   ├── dllmain.cpp         # DllMain + COM exports + Registration
│   └── AmsiProvider.def    # Export list cho linker
├── AmsiProvider.vcxproj    # Visual Studio 2022 project
├── Install-AmsiProvider.ps1 # Script cài đặt / test
└── README.md
```


---

## Cài đặt

```cmd
# Elevated Command Prompt
regsvr32 AmsiProvider.dll
regsvr32 /u AmsiProvider.dll
```

---

## Registry Keys được tạo

```
# COM Server registration
HKLM\SOFTWARE\Classes\CLSID\{2E5D8A62-77F9-4F7B-A90C-2744820139B2}
  (Default) = "Sample AMSI Monitoring Provider"
  
HKLM\SOFTWARE\Classes\CLSID\{2E5D8A62-77F9-4F7B-A90C-2744820139B2}\InProcServer32
  (Default)       = "C:\path\to\AmsiProvider.dll"
  ThreadingModel  = "Both"

# AMSI Enrollment
HKLM\SOFTWARE\Microsoft\AMSI\Providers\{2E5D8A62-77F9-4F7B-A90C-2744820139B2}
```

---

## Log File Format

Log được ghi tại `C:\AmsiProvider\amsi_log.txt`:

```
[2025-06-15 10:23:45]
  App     : PowerShell_C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe
  Content : <ScriptBlock>
  Session : 1234567890
  Size    : 512 bytes
  Data    :
  >>>
  Get-Process | Where-Object { $_.CPU -gt 10 }
  <<<
```

---

## Ưu tiên Provider

Khi có nhiều AMSI Providers, **thứ tự trong Registry không đảm bảo ưu tiên**.  
Windows Defender thường được gọi trước. Để test:

```powershell
# Xem các providers đang đăng ký
Get-ChildItem "HKLM:\SOFTWARE\Microsoft\AMSI\Providers"

# Tạm thời đổi tên provider khác để test (CHỈ TRÊN MÁY TEST)
# CẢNH BÁO: Không làm trên máy production vì sẽ tắt scanning thật
```

Từ Windows 10 1709+, **nhiều providers có thể cùng tồn tại** — tất cả đều được
gọi. Provider của bạn sẽ nhận được tất cả buffer scan mà không cần disable
Windows Defender.

---

## Thay đổi CLSID

Thay GUID `{2E5D8A62-77F9-4F7B-A90C-2744820139B2}` bằng GUID của riêng bạn.

## Tham khảo

- [Microsoft AMSI documentation](https://learn.microsoft.com/en-us/windows/win32/amsi/)
- [IAntimalwareProvider interface](https://learn.microsoft.com/en-us/windows/win32/api/amsi/nn-amsi-iantimalwareprovider)
- [Windows Classic Samples — AmsiProvider](https://github.com/microsoft/windows-classic-samples/tree/main/Samples/AmsiProvider)
