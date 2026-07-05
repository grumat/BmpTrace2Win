# BmpTrace2Win

A tool to link TraceSWO USB pipe output from the Black Magic Probe to the console or a Raw 
TCP port.


## Motivation

If you are using a **Black Magic Probe** and want to follow the available trace output using
a Windows PC you probably aren't taking advantage of the **TraceSWO** feature as the available 
tools are quite Linux-oriented.

I am not pleased about writing such a low-level support tool as I was expecting to use my 
hard-earned time to develop the https://github.com/grumat/glossy-msp430 firmware 
project that is still under heavy development. So I searched some alternatives before starting 
this, but really wasn't having luck with the current alternatives.

Three options I've tried had their issues, as described next.


### swolisten.c

The **Black Magic Probe** firmware offers a tiny tool here: 
https://codeberg.org/blackmagic-debug/blackmagic/src/branch/main/scripts/swolisten.c

Although this is a very interesting option, I don't know how to handle pipes in Windows or
even if it is supported there.

Nevertheless, its SWO decoder is used as a base in the code presented here.


### [bmtrace](https://codeberg.org/compuphase/Black-Magic-Probe-Book/src/branch/main/source)

This option was the best I could have hoped for — it really has a beautiful look.
But the initial attempts were very frustrating as there is a bug in the SWO
decoder that, depending on transmission errors, enters an infinite loop of messages
and the software stops responding. Not being able to stop or cancel it — and the fact
that it happens on every single debug session — made it impractical.

So the first thing I thought was to collaborate and fix the issue. But when I opened the source
code I saw a single **main()** function having **1131 lines of code**.
How can we in 2021 still see code like this? People need to know that
besides software having something to do with mathematics, logic or so, the main
characteristic of good software is not how *powerful* the *for loops* or *if branches*
look, but maintainability. Because software **does** have bugs that someone
**will** have to fix someday, usually consuming far more resources than it took to develop it
(or simply throw it away).

So I refuse to run software written like this on my PC. It's a principle. If
someone can't organize themselves in a structured way, they also can't help me.
No one would buy a car with a complete mess under the hood!


### Orbuculum

No Windows port.


## Description of the tool

The code here is a quick but *not so dirty* solution to a simple problem: I want to debug
my firmware using SWD in my brilliant VisualGDB extension in my beloved Visual Studio.
Alongside my debugger I want my trace output dumped directly to a Windows
console with full ANSI color support.

The tool reads from the BMP's TraceSWO USB device, formats messages with configurable
per-channel colors and labels, and writes them directly to the console using VT100
escape sequences (supported on Windows 10 Anniversary Update and later). Each SWO
channel can be assigned a **Title**, **foreground** and **background** color via the
`.ini` configuration file, making it easy to distinguish message types at a glance.

You can also use the tool in **TCP mode** (`-p <port>`) to pump raw output to a
network terminal such as PuTTY, but the primary mode of operation is direct console
output.

So don't expect other features. The tool will not configure the BMP for you, as
described in other docs about TraceSWO (including bmtrace).

You hit **[F5]**, VisualGDB will launch GDB with your ELF image, set up the BMP for you,
flash the firmware and start to run. If you have an instance of this tool attached to
the BMP's trace device, you will get your trace output. It's that simple.

Note that you could probably use the tool from Eclipse or VSCode, but the details
you will have to figure out yourself, as I see no reason to *downgrade* my almost perfect
development environment to check for other possibilities.


## Setup

These are the required steps:
- Use [Zadig](https://zadig.akeo.ie/) to install a WinUSB driver for the 
**Black Magic Trace Capture** device.
- Setup your firmware to enable TraceSWO.
- Setup VisualGDB to use the **Black Magic Probe** as your debugger, including TraceSWO.
- Run an instance of **BmpTrace2Win**.
- (Optional) Configure and use Putty to receive results via TCP mode.
- Run the debug session.


### Zadig setup

I won't enter the details on how to use Zadig as this has enough sources.
Just make sure to select the correct device (**Black Magic Trace Capture**) and use
only the WinUSB driver. Other libusb alternatives would probably fail as the tool
uses native WinUSB calls.


### Enable TraceSWO in your firmware

The best source is the [SWO documentation](https://codeberg.org/blackmagic-debug/pages/src/branch/main/content/docs/usage/swo.md)
from the official Black Magic Probe documentation.

Configure it in UART mode and do not increase the speed too much, as the BMP
is also busy running SWD while debugging your firmware. The values presented in
the documentation are far from realistic. The threshold stays around 900,000 bps. Personally
I ended up with 720,000 bps and consider it very reliable.

For beginners a good alternative is the [Debugging, Tracing & Programming with Black Magic](https://codeberg.org/compuphase/Black-Magic-Probe-Book/src/branch/main/BlackMagicProbe.pdf) book,
but I cannot recommend it for advanced users as the text is too lengthy.


### VisualGDB Setup (and maybe other IDEs)

On the **VisualGDB Project Properties** dialog select the **Debug Settings** tab.
As debug option select **Full-custom mode**.

Set the following fields:
- **Command:** Leave default value (`C:\SysGCC\arm-eabi\bin\arm-none-eabi-gdb.exe`).
- **Arguments:** Leave default value (`--interpreter mi $(TargetPath)`).
- **Working Directory:** According to your need (`$(TargetDir)`).
- **Environment:** According to your need.
- **Target:** Leave default value (`Local Computer`).
- **Run GDB Stub:** Not required (this is exactly what BMP deprecates).
- **Startup Delay:** Not required for most cases.
- **Target Selection Command:** `target extended-remote COM4`  
This is a crucial setting. In this example I use **COM4**, but the value depends on your 
computer configuration. Be careful if you have a COM10 or greater you should use Windows 
extended device notation: `\\.\COM10`.
- **After Selecting the Target:** `Start target with "run" command`

Then you need to set up advanced GDB connection commands by clicking on the
**Additional GDB Commands** tab.

On the "before selecting target" field enter:
```batch
set mem inaccessible-by-default off
```

Then on the "during target selection" field enter:
```batch
monitor swdp_scan
monitor traceswo enable 720000
attach 1
load
```

If you decide to use a different TraceSWO speed, review the settings above and enter
the appropriate value.


### Run BmpTrace2Win

This is a simple console program. Just run it from the command Prompt or double-click.

Some IDEs allow adding a custom command, which is also the case for Visual
Studio. Use whichever approach works best for you.

See the command line options below — the tool has two modes of operation.


### Putty (TCP mode only)

Putty is not required for normal use (direct console output is the default and 
recommended mode). However, if you need to view trace output on a remote machine or
prefer Putty's terminal emulation, the tool can serve a raw TCP stream. Open Putty 
and create a profile for your connection.

The following should be configured:
- For **connection type**, select `Other` and `Raw`.
- For **Host Name** type `127.0.0.1` or `localhost`.
- For the **Port** field enter `2332`
- Other interesting options are:
  - **Colors:** Enable all terminal colors, as **BmpTrace2Win** will use them
  - **Translation:** This is project dependent, but as a Windows user chances are 
  you are using a different code page, such as `Win 1252`.
  - **Terminal:** Windows typically need to enable **Implicit CR in every LF** so
  your '\n' end-lines will work in the expected way.

To interact with Putty terminal, you will need to start **BmpTrace2Win** differently 
and enable TCP mode, like in this example:

```batch
BmpTrace2Win -p 2332
```


### Running the Debug Session

Before starting the debug session, start **BmpTrace2Win**. In TCP mode the tool 
waits for a client (e.g. Putty) to connect before opening the BMP Trace device.

> In TCP mode, Windows firewall will notify you that the **BmpTrace2Win** tool is 
> trying to reach the network. Don't expect functionality if you deny access.

Note that tracing will only work after the GDB session is launched by VisualGDB,
because the following conditions are required for TraceSWO:
- SWD mode must be active (command `monitor swdp_scan`)
- SWO must be enabled (command `monitor traceswo enable <speed-bps>`)

> If you switch the order of these commands, chances are that TraceSWO will fail.

> Another thing to consider is that if you are experimenting with different SWO speeds,
> communication noise may occur until BMP and firmware are working at the same speed.


## Command line options


These are the command line options of **BmpTrace2Win**:
- **-h**: Display a help message and stop the program
- **-c \<ini-file\>**: Specify a path to a custom `.ini` file with the color
configuration for your trace output. The tool looks for a file with the same
name as the executable and a `.ini` extension in the same directory as the executable.
A sample `.ini` file is provided in the repository.
- **-m**: Monochrome mode. Use this if your terminal does not support VT100
colors. Newer Windows 10 versions accept them, so this is only useful if you use an old
Windows installation.
- **-p \<nnn\>**: Enable TCP mode. A raw text protocol pumps messages to the specified
port number. Please ensure the firewall allows you to access the port.
- **-v**: Increase the verbose level in TCP mode. More debug messages are displayed
on the console. This option has no effect without TCP mode, as it would disturb the
intended trace operation.


## Configuration (.ini file)

The tool reads a `.ini` file (by default `BmpTrace2Win.ini` next to the executable)
that defines per-channel colors and titles, plus general options. Each SWO channel
0–31 can be configured in its own `[ChannelN]` section:

```ini
[General]
; Reset escape sequence to return to default terminal colors
Reset=^[0m
; Path to the log file. If not provided, defaults to %TEMP%\BmpTrace2Win.log
; Terminal output is logged without ANSI color codes, using the Title as line head.
;LogFile=%TEMP%\BmpTrace2Win.log

[Channel0]
Title=Msg
Fore=^[97m         ; bright white foreground
Back=^[42m         ; green background

[Channel1]
Title=Err
Fore=^[97m         ; bright white foreground
Back=^[41m         ; red background

[Channel2]
Title=Dbg
Fore=^[97m         ; bright white foreground
Back=^[45m         ; magenta background
```

The `^` character is used as a shorthand for the ASCII ESC character (0x1B).
For example `^[97m` becomes `ESC[97m` — a VT100 SGR code for bright white text.
Channels without a section fall back to the default foreground and background
defined in the `LoadIni()` method of `AppConfig.cpp`.

The sample `.ini` included in the repository provides a complete configuration
for all 32 channels.

## Log file

The tool can dump all terminal output to a plain text log file, with ANSI escape
sequences stripped and the channel Title prepended as a line header. Configure
it via the `LogFile` key in the `[General]` section of the `.ini`:

```ini
[General]
LogFile=C:\path\to\my_trace.log
```

If `LogFile` is not set, it defaults to `%TEMP%\BmpTrace2Win.log`. Set it to an
empty value to disable file logging entirely.

The log file is opened with file-sharing enabled (`_SH_DENYNO`), so it can be
read by other tools (e.g. Notepad++, `tail -f`) while BmpTrace2Win is running.
The logger's own diagnostic messages (errors, warnings) are also written to the
same file.


## Known Limitations / Missing Features

As this is a *one weekend project* it has a very small feature set. These are the points
that surely need more attention.
- A command to set up the BMP for TraceSWO would be handy, as a cold boot disables the feature
and there is no independent way to re-enable it.
- Add an option to specify a host name and pump SWO messages to a remote PC.
- BMP hardware varies a lot, and if you are using a Chinese clone your
hardware might miss a resistor connecting the TDO pin to the USART RX pin of the MCU. If
that is the case, you will need a talented technician able to solder
0.5 mm pitch MCU pins.

> TODO


