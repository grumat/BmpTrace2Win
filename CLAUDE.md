# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Windows-only Visual Studio C++ console app. Targets the v143 toolset, Windows SDK 10.0, Unicode character set. Configurations: Debug/Release × Win32/x64.

```powershell
# Build from the command line (requires "Developer PowerShell for VS")
msbuild BmpTrace2Win.sln /p:Configuration=Release /p:Platform=x64
```

There is no test suite, no linter config, and no CI. The project depends on ATL (`atlbase.h`, `atlstr.h`, `atlpath.h`, `CRBMap`, etc.) and links against WinUSB / SetupAPI / WinSock2.

Bumping the version: edit `APP_VER` / `APP_REV` in `BmpTrace2Win/version.h` (commit history shows version bumps are their own commits, e.g. `a15130d`).

## Runtime prerequisites

The tool only works against a real Black Magic Probe with a WinUSB driver bound to the **Black Magic Trace Capture** interface (use Zadig). Without that, `CBmpSwo::Open()` cannot find the device. See README.md for the full VisualGDB / firmware setup — those steps are not reproducible from code alone.

Two run modes (selected at startup, no runtime switching):
- **Terminal mode** (default): writes VT100-colored output to the console. `EnableVTMode()` is called so Windows 10+ renders ANSI sequences.
- **TCP mode** (`-p <port>`): blocks in `CTelnetLink::Serve()` until a raw client (e.g. PuTTY in Raw mode on `127.0.0.1:<port>`) connects, *then* opens the USB device.

`-v` only has effect in TCP mode (in terminal mode log level is forced to `ERROR_LEVEL` to avoid corrupting trace output).

## Architecture

The data path is a one-way pipeline: **WinUSB pipe → SWO/ITM decoder → formatter → sink**. The seams between stages are small interfaces in `SwoPayload.h`, which keeps the decoder independent of where output goes.

- `CBmpSwo` (`BmpSwo.{h,cpp}`) — owns the WinUSB device. `Open()` enumerates USB interfaces, picks the one matching `VID=0x1d50 / PID=0x6018 / MI=0x05`, and resolves IN/OUT pipes. `DoTrace()` is the read loop; each byte is fed to `PumpData()`, a small state machine (`ITM_IDLE / ITM_SYNCING / ITM_TS / ITM_SWIT`) that reassembles ITM timestamp + SWIT packets into `SwoMessage` records. `OPT_AUTO_FLUSH` is a compile-time flag (default 0) for flushing partial lines on a timeout — currently disabled because the BMP, not this tool, is the source of stuck-byte issues (see commit `47f5270`).
- `ISwoFormatter` / `SwoFormatter` (in `BmpTrace2Win.cpp`) — converts a `SwoMessage` (channel + ms-resolution timestamp + text) into a `CStringA` with VT100 color escapes pulled from `CAppConfig`.
- `ISwoTarget` — sink interface implemented by:
  - `CTerminalLink` — writes formatted strings to stdout.
  - `CTelnetLink` — accepts a single raw TCP client and forwards bytes; verbose logging is gated to this mode.
- `CAppConfig` (`AppConfig.{h,cpp}`) — loads `BmpTrace2Win.ini` (per-channel foreground/background/title color + monochrome override). The ini path defaults to `<exe-dir>\BmpTrace2Win.ini`; `-c <path>` overrides. `-m` forces monochrome regardless of ini.
- `Logger::TheLogger()` (`TheLogger.{h,cpp}`) — global log singleton; level set once in `main` from build type + `-v` count.
- `WinUSBWrappers/` — third-party header-only wrappers (Naughter Software). Treat as vendored — don't refactor.

Lifetime: `main` constructs the sink first, then opens the USB device, sends a banner `SwoMessage`, runs `DoTrace` until the read loop ends, and tears both down. `CAtlException` from any ATL call is caught at the top level and printed via `_com_error`.

## Conventions worth knowing before editing

- The codebase uses ATL types throughout (`CString`, `CStringA`, `CPath`, `CRBMap`, `CTime`). Don't reach for STL equivalents in new code in this repo — the existing code deliberately avoids `std::set` etc. (see comment in `BmpSwo.h`).
- Source files are saved in a Windows codepage (not UTF-8); some comments contain non-ASCII characters that look mangled in UTF-8 viewers. Preserve the existing encoding when editing — don't "fix" the comments.
- The `.ini` color scheme is part of the user-facing contract; channel numbers in the ini map directly to ITM stimulus port numbers in firmware.
