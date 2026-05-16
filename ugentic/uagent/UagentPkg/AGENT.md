# UagentPkg Agent Notes

## What this project is

`UagentPkg` is a small EDK2 UEFI application that:

- starts after PXE boot
- prefers PXE-derived IPv4 configuration and falls back to self-DHCP with `EFI_IP4_CONFIG2_PROTOCOL`
- connects to a fixed remote server at `192.168.70.1:8080`
- runs a server-driven remote session over a custom TCP packet protocol
- falls back to a local shell if the remote session cannot start

This is not a normal userspace socket program. It depends on UEFI PXE and TCP4 protocols being present on the booted machine.
The connection order is `PXE first, self-DHCP fallback second`.

## Project structure

- `Uagent.c`
  - UEFI entry point
  - disables watchdog
  - starts the remote session first
  - installs the debug protocol used by other EFI apps to send text back to the server

- `Shell.c`
  - local fallback shell
  - remote-session loop and remote command dispatcher
  - exposes `connect` and `netstatus` in the local shell
  - handles commands such as `help`, `status`, `echo`, `disconnect`, `reboot`
  - handles uploaded EFI apps and execution

- `TcpClient.c`
  - tries PXE IPv4 state first
  - scans TCP4 + IP4Config2 NIC candidates when PXE state is missing
  - requests DHCP through `EFI_IP4_CONFIG2_PROTOCOL`
  - opens TCP4 service binding
  - creates and configures a TCP4 child
  - sends and receives packets
  - cleans up the connection

- `Uagent.h`
  - shared constants, enums, structs, and function declarations

- `Uagent.inf`
  - EDK2 module definition

- `UagentPkg.dsc`
  - package/platform build description

## Boot and runtime flow

1. `UefiMain()` in `Uagent.c` starts the app.
2. The app immediately tries `RunRemoteSession()`.
3. `InitSocketClient()` in `TcpClient.c` tries `PXE first, self-DHCP fallback second`, then opens TCP4 on the selected NIC.
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

- PXE state is missing
- or `EFI_TCP4_SERVICE_BINDING_PROTOCOL` / `EFI_IP4_CONFIG2_PROTOCOL` is missing on candidate NICs
- or DHCP fallback times out without a lease

When that happens, the app fails before any packets appear in Wireshark.

## Editing guidance for future agents

- Keep the packet protocol stable unless the Go server is updated too.
- Treat `TcpOutputText` as the canonical way to send user-visible text to the remote server.
- Be careful with changes in `TcpClient.c`; this code depends on firmware protocol availability, not only on network reachability.
- If debugging connection failures, add stage-specific status prints around PXE discovery, TCP4 service binding, configure, and connect.
