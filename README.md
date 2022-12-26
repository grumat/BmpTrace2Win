# BmpTrace2Win

A tool to link TraceSWO USB pipe output from the Black Magic Probe to the console or a Raw 
TCP port.


## Motivation

If you are using a **Black Magic Probe** and wants to follow the available trace output using
a Windows PC you probably aren't taking benefit of the **TraceSWO** feature as the available 
tools are quite Linux-oriented.

I am really not pleased on writing such a low level support tool as I was expecting to use my 
hard earned time to develop the https://github.com/grumat/glossy-msp430 firmware 
project that is still under heavy development. So I searched some alternatives before starting 
this, but was really not having luck with the current alternatives.

Three options I've tried, had its issues as described next.


### swolisten.c

The **Black Magic Probe** firmware offers a tiny tool here: 
https://github.com/blacksphere/blackmagic/blob/master/scripts/swolisten.c

Although this is a very interesting option I don't know how to handle pipes in Windows and
even know how it is supported.

But the tool is interesting as the SWO decoder is used as base in the code presented here.


### [bmtrace](https://github.com/compuphase/Black-Magic-Probe-Book)

This option was really the best I could expect as it really has a beautiful look.
So the initial attempts was very frustrating as there is some kind of bug in the SWO
decoder that, depending on transmission errors it enter an infinite loop of messages and 
the software stops responding. Not being able to stop/cancel and a problem that happens on 
every single debug session, turned it impractical.

So, the first I though was to collaborate and fix the issue. So, when I opened the source 
code I saw a single **main()** function having **1131 lines of code**!!! I say sorry.
How can we in 2021 still hit in newly written code like this? People need to know that
besides software coding has something to do with mathematics, logic or so, the main 
characteristic of a good software is not how *powerful* the *for loops* or *if branches* 
looks like, but it is the maintenance. Because software **DO** have bugs that someone 
**WILL** have to fix someday, usually consuming far more resources than to develop it
(Or simply throw it away).

So I deny myself to execute a software written like this on my PC. It's a principle. If 
someone can't organize himself in a structured way, he/she also can't help me. 
No one will buy a car with a complete mess under the hood!


### Orbuculum

No Windows port.


## Description of the tool

The code here is a quick but *not so dirty* solution to a simple problem: I want to debug
my firmware using SWD in my brilliant VisualGDB extension in my beloved Visual Studio.
At one side of my desktop I have a console or Putty terminal where my trace output should 
be dumped.

So don't expect other features. The tool will not configure the BMP for you, such as 
described in other docs about TraceSWO (including bmtrace).

You hit **[F5]** VisualGDB will launch GDB with your ELF image, setup the BMP for you,
flash the firmware and start to run. If you have an instance of this tool attached to
the BMP's trace device and a Putty instance linked to it, you will get your trace output.
Just that simple.

Note that you could probably use the tool in an Eclipse or VSCode IDE, but the details
you will have to try yourself, as I see no reason to *downgrade* my almost perfect 
development environment to check for other possibilities.


## Setup

This are the required steps:
- Use [Zadig](https://zadig.akeo.ie/) to install a WinUSB driver for the 
**Black Magic Trace Capture** device.
- Setup your firmware to enable TraceSWO.
- Setup VisualGDB to use the **Black Magic Probe** as your debugger, including TraceSWO.
- Run an instance of **BmpTrace2Win**.
- Optionally configure and use Putty to receive results.
- Run the debug session.


### Zadig setup

I won't enter the details on how to use Zadig as this hs enough sources.
Just take care to select the correct device (**Black Magic Trace Capture**) and use
only the WinUSB driver. Other libusb alternatives would probably fail as the tool
uses native WinUSB calls.


### Enable TraceSWO in your firmware

The best sources is the [UsingSWO](https://github.com/blacksphere/blackmagic/blob/master/UsingSWO)
of the official black magic probe repository.

Just make the setup in UART mode and do not increase your speed too much, as the BMP 
is also busy running the SWD while debugging your firmware and values presented in 
the text are far from realistic. The threshold stays around 900,000 mbps. Personally
I ended up with 720,000 mbps and consider it very reliable.

For beginner a good alternative is the [Debugging, Tracing & Programming with Black Magic](https://github.com/compuphase/Black-Magic-Probe-Book/blob/master/BlackMagicProbe.pdf),
but I cannot recommend for advanced users as the text is too lengthy.


### VisualGDB Setup (and maybe other IDE's)

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

Then you need to setup advanced GDB connection commands, by clicking on the 
**Additional GDB Commands** tab.

On the "before selecting target" field enter:
```batch
set mem inaccessible-by-default off
```

Then on the "during target selection" field enter:
```batch
monitor swdp_scan
monitor traceswo 720000
attach 1
load
```

If you decide to use other TraceSWO speed, you should review the settings above and enter
your value.


### Run BmpTrace2Win

This is a simple console program. Just call it using command Prompt or double-click.

Some IDE's allow the addition of a custom command, which is also the case of Visual 
Studio. Apply the solution that is more easy for you.

See later all console options, as this tool has two modes of operation.


### Putty

Putty is not required for the normal use, but if you opt to do so, some adjustments 
are required. Open Putty and create a profile for your connection.

The following should be configured:
- For **connection type**, select `Other` and `Raw`.
- For **Host Name** type `127.0.0.1` or `localhost`.
- For the **Port** field enter `2332`
- Other interesting options are:
  - **Colors:** Enable all terminal colors, as **BmpTrace2Win** will use them
  - **Translation:** This is project dependant, but as Windows user chances are that 
  you are using other code page, such as `Win 1252`.
  - **Terminal:** Windows typically need to enable **Implicit CR in every LF** so
  your '\n' end-lines will work in the expected way.

To interact with Putty terminal, you will need to start **BmpTrace2Win** differently 
and enable TCP mode, like in this example:

```batch
BmpTrace2Win -p 2332
```


### Running the Debug Session

Before starting the debug session, you should start **BmpTrace2Win** followed by 
**Putty**. The BmpTrace2Win waits until Putty connects to connect to the 
BMP Trace output.

> Windows firewall will notify you that the **BmpTrace2Win** tool is trying to 
> reach the network. Don't expect functionality if you deny access.

Note that tracing will only work after the GDB session is launched by VisualGDB, 
because the following condition is required for TraceSWO:
- SWD mode (command `monitor swdp_scan`)
- enabling SWO (command `monitor traceswo <speed-bps>`)

> If you switch the order of theses command chances are that TraceSWO will fail.

> Another thing to consider is that if you are experimenting different SWO speeds
> communication noise may happen until BMP and firmware are working in the same speed.


## Command line options


These are the command line option of **BmpTrace2Win**:
- **-h**: Display a help message and stops the program
- **-c \<ini-file\>**: Allows you to specify a path of the `.ini` file, having the color 
configuration of your trace output. Note that the tool searches a file with the same 
name of the executable and the `.ini` extension in the same directory of the executable. 
A sample `.ini` file was provided in the repository.
- **-m**: Enter monochrome mode. This is useful if your terminal does not support VT100
colors. Newer Windows 10 versions accepts them, so this is useful if you use an old
Windows installation.
- **-p \<nnn\>**: Enables TCP mode. A raw text protocol pumps messages to the specified 
port number. Please ensure that the firewall allows your to access the port.
- **-v**: Increases the verbose level in the TCP mode. More debug messages are displayed 
on the console. This option has no effect without the TCP mode, as it disturbs the 
intended trace operation.


## Known Limitations / Missing Features

As this is a *one weekend project* it has a very small feature set. These are the points
that needs surely more attention.
- A command to setup the BMP for traceSWO would be handy, as a cold boot stops the feature
and there is no independent way to accomplish this.
- Add an option to specify a host name and pump SWO messages to a remote PC.
- BMP hardware varies too much and if you are using a Chinese clone maybe your 
hardware misses a resistor connecting TDO pin to the USART RX pin of the MCU. If
this is your case, you will need a talented technician able to solder these 
0.5 mm pins of the MCU.

> TODO


