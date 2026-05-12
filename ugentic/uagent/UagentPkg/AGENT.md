# UagentPkg Agent Notes

## What this project is

`UagentPkg` is a small EDK2 UEFI application that:

- starts after PXE boot
- reuses the NIC's DHCP-provided IPv4 configuration
- connects to a fixed remote server at `192.168.70.1:8080`
- runs a server-driven remote session over a custom TCP packet protocol
- falls back to a local shell if the remote session cannot start

This is not a normal userspace socket program. It depends on UEFI PXE and TCP4 protocols being present on the booted machine.

## Project structure

- `UagentPkg/Uagent.c`
  - UEFI entry point
  - disables watchdog
  - starts the remote session first
  - installs the debug protocol used by other EFI apps to send text back to the server

- `UagentPkg/Shell.c`
  - local fallback shell
  - remote-session loop and remote command dispatcher
  - handles commands such as `help`, `status`, `echo`, `disconnect`, `reboot`
  - handles uploaded EFI apps and execution

- `UagentPkg/TcpClient.c`
  - finds PXE state
  - opens TCP4 service binding
  - creates and configures a TCP4 child
  - sends and receives packets
  - cleans up the connection

- `UagentPkg/Uagent.h`
  - shared constants, enums, structs, and function declarations

- `UagentPkg/Uagent.inf`
  - EDK2 module definition

- `UagentPkg/UagentPkg.dsc`
  - package/platform build description

- `UagentPrintTcpExPkg/`
  - standalone test package that locates the custom debug protocol
  - builds `UagentDebugTest.efi`

- `builds/`
  - shared output directory for local package builds in this repo

## Build environment

This repo is not the EDK2 workspace root. The actual EDK2 workspace used here is:

- `WORKSPACE=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2`
- `EDK_TOOLS_PATH=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2/BaseTools`
- `CONF_PATH=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2/Conf`

In a fresh shell, `build` may not exist until EDK2 setup is sourced:

```sh
cd /home/alexa/Documents/SanderStuff/aau/cyber2/edk2
source edksetup.sh
```

After that, build packages from this repo by pointing `PACKAGES_PATH` at the repo root:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent
```

Do not assume relative `-p UagentPkg/...` paths will resolve to this repo unless `PACKAGES_PATH` is set correctly.

## Build commands

Build the main Uagent app:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent build \
  -p /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentPkg/UagentPkg.dsc \
  -m UagentPkg/Uagent.inf \
  -a X64 -b DEBUG -t GCC5
```

Build the debug test app:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent build \
  -p /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentPrintTcpExPkg/UagentDebugTestPkg.dsc \
  -m UagentPrintTcpExPkg/UagentDebugTest.inf \
  -a X64 -b DEBUG -t GCC5
```

Expected outputs:

- `builds/UagentPkg/DEBUG_GCC5/X64/Uagent.efi`
- `builds/UagentDebugTestPkg/DEBUG_GCC5/X64/UagentDebugTest.efi`

EDK2 on Linux may emit a harmless final warning about copying `*.pdb` files. That does not mean the `.efi` build failed if the `.efi` file exists.

## Boot and runtime flow

1. `UefiMain()` in `Uagent.c` starts the app.
2. The app immediately tries `RunRemoteSession()`.
3. `InitSocketClient()` in `TcpClient.c` reuses PXE DHCP IPv4 settings and opens TCP4.
4. If connection succeeds, the EFI app enters a persistent remote-command loop.
5. If connection fails, the app drops to the local shell and the `connect` command can retry.

## Most important rule: printing to the server

Do not assume that printing to the UEFI console means the server will see the text.

Server output must go through the custom TCP command protocol.

- To send text back to the server, use `TcpOutputText`.
- Do not send raw console text and expect the Go server to interpret it.
- Do not invent a new packet format unless both client and server are updated together.

The current packet format is:

- `[command:1][payload_length:2][payload:N]`

Command IDs are:

- `1 = TcpSendText`
- `2 = TcpGetApps`
- `3 = TcpConnectSession`
- `4 = TcpOutputText`
- `5 = TcpDisconnectSession`
- `6 = TcpPushEfiApp`
- `7 = TcpExecuteEfiApp`
- `8 = TcpEchoText`

## How text should be sent back

There are two main correct paths:

- Remote shell responses in `Shell.c`
  - build a `TCP_COMMAND`
  - set `Type = TcpOutputText`
  - put the text in `Text`
  - send it with `SendCommandPacket()`

- Programmatic debug output from other EFI code
  - use the installed `UAGENT_DEBUG_PROTOCOL`
  - `UagentSendDebugMessage()` in `Uagent.c` converts the message into a `TcpOutputText` packet

If an agent changes anything related to output, preserve this behavior.

## Server-side assumptions

The paired server is the Go project at:

`/home/alexa/Documents/SanderStuff/aau/cyber2/userve/uservego`

Important behavior:

- `echo test` on the Go server becomes `TcpEchoText` with payload `test`
- commands that EFI should parse as shell instructions must be sent as `TcpSendText`

## Current remote commands supported by EFI

- `TcpSendText`
  - `help`
  - `status`
  - `echo <text>`
  - `disconnect`
  - `reboot`

- `TcpEchoText`
  - send payload back directly

- `TcpGetApps`
  - returns `No local app registry implemented`

- `TcpDisconnectSession`
  - closes the session

- `TcpPushEfiApp`
  - stores an uploaded EFI binary in memory

- `TcpExecuteEfiApp`
  - loads and starts the most recently uploaded EFI app

## Machine-specific networking warning

This project only works on machines where UEFI exposes the needed network stack.

The common failure pattern is:

- PXE exists
- but `EFI_TCP4_SERVICE_BINDING_PROTOCOL` is missing or unsupported on that NIC

When that happens, the app fails before any packets appear in Wireshark.

## Editing guidance for future agents

- Keep the packet protocol stable unless the Go server is updated too.
- Treat `TcpOutputText` as the canonical way to send user-visible text to the remote server.
- Be careful with changes in `TcpClient.c`; this code depends on firmware protocol availability, not only on network reachability.
- If debugging connection failures, add stage-specific status prints around PXE discovery, TCP4 service binding, configure, and connect.
