# Uagent Test-App Agent Guide

## Purpose

This repo contains small EDK2 UEFI test applications that send remote-visible debug text through the `Uagent` custom debug protocol.

The main test package here is:

- `uagent/UagentPrintTcpExPkg`

It exists so an agent can quickly create or modify EFI test apps without needing to understand the full `UagentPkg` implementation.

## What The Agent Should Do Here

Use `uagent/UagentPrintTcpExPkg` when the task is one of these:

- create a new EFI test app that sends a debug message to the active `Uagent` server session
- change the message text sent by a test app
- add another debug send
- add simple local `Print()` confirmation text for the operator
- wire a new test app into the EDK2 build through `.inf` and `.dsc`
- build, deploy, and run a test EFI through the existing `userve` service

## Operator And Agent Runtime Split

The `userve` tooling is now split into two binaries:

- `userve/bin/userver`
- `userve/bin/ucli`

Important runtime rule:

- the human operator starts and owns `userver`
- the coding agent must not start, restart, or manage `userver`
- the coding agent should assume `userver` is already running when deploy/run work is requested
- the coding agent should use `ucli` to interact with the already-running `userver`

The agent-side control path is:

1. build or update the EFI app
2. upload the built `.efi` with `ucli push`
3. ask the target to execute it with `ucli run`
4. inspect server-visible output with `ucli outputs` or `ucli status` when needed

## What The Agent Must Not Do

- do not change the `Uagent` packet format here
- do not invent a new network transport
- do not assume `Print()` or `DEBUG()` is visible to the Go server
- do not replace the `UAGENT_DEBUG_PROTOCOL` path with raw TCP code in this package
- do not change GUIDs unless the owning protocol definition changes intentionally
- do not create a custom uploader or runner when `userve` already provides `push` and `run`
- do not start `userver` as part of task execution unless the user explicitly asks for that

## Dependency Contract

This package depends on `UagentPkg` providing the custom protocol defined in:

- `../UagentPkg/Uagent.h`

The key contract is:

- protocol GUID: `UAGENT_DEBUG_PROTOCOL_GUID`
- protocol type: `UAGENT_DEBUG_PROTOCOL`
- remote send method: `SendDebugMessage()`

This package does not own the transport. It only consumes the protocol.

## Core Rule: Remote Output

UEFI console output is not server output.

These only print locally:

- `Print()`
- `DEBUG()`

If text must be visible on the Go server, the EFI app must:

1. locate `UAGENT_DEBUG_PROTOCOL`
2. call `SendDebugMessage()`

That call is the approved path because `UagentPkg` converts it into a `TcpOutputText` packet for the server.

Agents must not treat local `Print()` success as proof that the message reached the server. Remote success means `SendDebugMessage()` was called and the resulting text is visible in the live `userve` server output.

Current transport behavior:

- `UagentPkg` now sends `TcpConnectSession` and `TcpOutputText` payloads as raw `CHAR16` bytes instead of squeezing them through the old fixed ASCII buffer
- `userve` decodes those payloads for display
- if a payload is not readable text, `userve` may display unreadable raw text

## Standard Implementation Recipe

When asked to make an EFI app in this package that sends a message to the server, do this:

1. Create or update a `.c` file with `UefiMain()`.
2. Include:
   - `<Uefi.h>`
   - `<Library/UefiApplicationEntryPoint.h>`
   - `<Library/UefiBootServicesTableLib.h>`
   - `<Library/UefiLib.h>`
   - `../UagentPkg/Uagent.h`
3. Define a local GUID variable from `UAGENT_DEBUG_PROTOCOL_GUID`.
4. Call `gBS->LocateProtocol()`.
5. If protocol lookup fails:
   - print the status locally with `Print()`
   - return the failure status
6. If lookup succeeds:
   - call `Debug->SendDebugMessage(Debug, L"...message...")`
   - print the returned status locally with `Print()`
   - return that status
7. Ensure the app has a valid `.inf`.
8. Ensure the package `.dsc` includes the component.

For any non-trivial test app, also send remote-visible marker messages:

- one clear start marker through `SendDebugMessage()`
- one clear success or completion marker through `SendDebugMessage()`
- one remote failure marker if the app aborts after protocol lookup succeeds

The marker text should be distinctive enough that an operator can recognize it immediately in the live `userve` output.

## Required File Pattern

For each new EFI test app, the expected files are:

- `Name.c`
- `Name.inf`

The package must also include the module in:

- `UagentDebugTestPkg.dsc`

If the request is only to change message text, usually only the `.c` file needs to change.

## Minimal Known-Good Template

Use this pattern unless the task requires something more specific:

```c
#include <Uefi.h>

#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include "../UagentPkg/Uagent.h"

STATIC EFI_GUID  mUagentDebugProtocolGuid = UAGENT_DEBUG_PROTOCOL_GUID;

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                  Status;
  UAGENT_DEBUG_PROTOCOL  *Debug;

  Status = gBS->LocateProtocol (
                  &mUagentDebugProtocolGuid,
                  NULL,
                  (VOID **)&Debug
                  );
  if (EFI_ERROR (Status)) {
    Print (L"LocateProtocol failed: %r\n", Status);
    return Status;
  }

  Status = Debug->SendDebugMessage (
                    Debug,
                    L"Hello from UagentDebugTest"
                    );
  Print (L"SendDebugMessage returned: %r\n", Status);
  return Status;
}
```

When adapting this template, prefer a distinctive remote message like:

```c
Status = Debug->SendDebugMessage (
                  Debug,
                  L"[UagentDebugTest] start"
                  );
```

and send a matching completion line before returning success.

## `.inf` Requirements

Each module `.inf` should declare:

- `MODULE_TYPE = UEFI_APPLICATION`
- `ENTRY_POINT = UefiMain`
- the source `.c` file
- package dependency on `MdePkg/MdePkg.dec`

It should link these library classes unless the module has a clear reason not to:

- `UefiApplicationEntryPoint`
- `UefiBootServicesTableLib`
- `UefiLib`

## `.dsc` Requirements

The package `.dsc` must:

- define a valid EDK2 platform
- include `MdePkg/MdePkg.dec
- provide the standard UEFI library implementations needed by the .inf
- list each EFI app under [Components]

If a new module is added and not listed in `[Components]`, the build will not include it.

# Build And Environment

This repo is not the EDK2 workspace root.

The EDK2 workspace used in this environment is:

- `WORKSPACE=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2`
- `EDK_TOOLS_PATH=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2/BaseTools`
- `CONF_PATH=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2/Conf`

The repo root for these packages is:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent`

The most important rule for building from this repo is:

- `PACKAGES_PATH` must point at `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent`

Without that, EDK2 may resolve package paths against the main workspace and build the wrong files or fail to find local ones.

If `build` is not available in the shell yet, initialize EDK2 first:

```sh
cd /home/alexa/Documents/SanderStuff/aau/cyber2/edk2
source edksetup.sh
```

There is no repo-local helper script that should be relied on here.

## `userve` Control Tooling

The `ucli` binary for agent use is located at:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli`

The source for that tool is located at:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/cmd/ucli/main.go`

Typical commands:

```sh
/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli status
/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli push /absolute/path/to/App.efi
/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli run
/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli outputs
```

Notes for agents:

- `ucli push` accepts a path to a local file and uploads the basename to the remote side
- `ucli run` triggers execution of the selected app on the connected target
- `ucli send "<text>"` exists for raw text commands, but normal EFI app deploy/run work should use `push` and `run`
- `ucli outputs` is the quickest way to inspect server-visible output after a run
- if `ucli status` reports `connected=false`, the target is not connected and deployment cannot proceed
- if `ucli` returns an error about no connected system, stop and report that state instead of inventing a fallback transport

The correct build pattern is to set `PACKAGES_PATH` inline for the build command:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent build ...
```

## Build Recipe

Build the existing debug test app with:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent build \
  -p /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentPrintTcpExPkg/UagentDebugTestPkg.dsc \
  -m UagentPrintTcpExPkg/UagentDebugTest.inf \
  -a X64 -b DEBUG -t GCC5
```

Build the main `Uagent` app with:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent build \
  -p /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentPkg/UagentPkg.dsc \
  -m UagentPkg/Uagent.inf \
  -a X64 -b DEBUG -t GCC5
```

Expected output locations:

- `uagent/builds/UagentDebugTestPkg/DEBUG_GCC5/X64/UagentDebugTest.efi`
- `uagent/builds/UagentPkg/DEBUG_GCC5/X64/Uagent.efi`

## Deploy And Run Recipe

After building an EFI app, the expected deploy/run flow is:

```sh
/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli push \
  /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/builds/UagentDebugTestPkg/DEBUG_GCC5/X64/UagentDebugTest.efi

/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli run

/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli outputs
```

Use the matching built `.efi` path for whatever module was requested.

Important detail:

- these packages are configured to write build products into the shared `uagent/builds/` directory, not into EDK2's default `Build/` folder

If a package builds successfully on Linux, EDK2 may still print a final warning about copying `*.pdb` files. That warning is harmless if the `.efi` file exists.

Expected behavior:

- local EFI console shows `SendDebugMessage returned: Success`
- Go server shows the remote text
- remote text may include Unicode-capable `SendDebugMessage()` content, not just short ASCII strings

## `userve` Workflow

When an agent needs to deploy and run a newly built EFI against the active remote `Uagent` session, use the `userve` server that is part of this repo.

The server project lives at:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve`

Build it with:

```sh
cd /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve
make
```

That produces:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/userver`
- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli`

The human operator is responsible for starting `userver`.

If the user explicitly asks how the operator should run it, the command is:

```sh
cd /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve
./bin/userver
```

Agents must not rely on an interactive `userve>` prompt. That shell no longer exists.

For agent-controlled deployment and execution, use:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli status`
- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli push /full/path/to/file.efi`
- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli run`
- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli outputs`
- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli send "text"`
- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve/bin/ucli disconnect`

Do not invent a second uploader or control path when `userve` already supports `push` and `run`.

Do not stop at "the EFI built successfully" or "the local console printed success". The agent must verify the remote side when the task is about server-visible output.

## Deployment Commands

From `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/userve`, the standard remote deployment flow is:

1. Assume the operator already started `./bin/userver`.

2. Wait for the remote `Uagent` client to connect. The server will print:

- `System connected from ...`
- `Session ready: ...`

3. Push the built EFI with `ucli`:

`./bin/ucli push /full/path/to/<ModuleName>.efi`

4. Execute the uploaded EFI:

`./bin/ucli run`

5. Inspect remote output with:

`./bin/ucli outputs`

Important behavioral rules:

- this `userve` implementation supports one active session, not a multi-target selector
- `push` sends the file basename to the remote side, so the `.efi` filename matters
- `run` executes the currently uploaded EFI on the active remote connection
- `ucli send "<text>"` sends ASCII text to the remote client
- remote text from EFI is available through `ucli outputs` and is also printed by `userver`
- if the task is to send text to the server, the agent should expect to see a distinctive remote marker line during `run`
- if no distinctive remote marker exists in the EFI code yet, the agent should add one

## Remote Execution Contract

The expected remote text path is:

1. the EFI app calls `SendDebugMessage()`
2. `UagentPkg` converts that into `TcpOutputText`
3. for `TcpConnectSession` and `TcpOutputText`, `UagentPkg` sends the text payload as raw `CHAR16` bytes
4. `userve` decodes the payload and exposes it through `ucli outputs` and the running `userver` log

This means:

- `Print()` is for the local EFI console
- `SendDebugMessage()` is for the remote Go server
- `SendDebugMessage()` is no longer limited by the old fixed ASCII payload path used for local control text commands

If the user asks the agent to "deploy the EFI", "send it to the server", "run it remotely", or "test it through Uagent", the agent should assume the correct path is:

1. build the `.efi`
2. assume `./bin/userver` is already running
3. use `./bin/ucli push /full/path/to/file.efi`
4. use `./bin/ucli run`
5. verify that remote output contains the expected distinctive `SendDebugMessage()` text

If the task is specifically about proving that output reached the server, the EFI should emit:

- a clear start message
- the main remote payload message or messages
- a clear completion or success message

## Common Failure Modes

`LocateProtocol failed`

- `Uagent.efi` is not running
- the protocol was not installed
- the session ended before the test app was launched

`SendDebugMessage returned: Not Ready`

- the debug protocol exists, but `UagentPkg` does not currently have an active TCP client

Message appears locally but not on server

- the app used `Print()` only
- the app never called `SendDebugMessage()`
- the app called `SendDebugMessage()` but did not emit any distinctive remote marker, so the operator could not tell what was sent

Build succeeds but `.efi` is missing

- wrong `.dsc`
- component missing from `[Components]`
- wrong `.inf` path in build command

`./bin/userver` shows no connection

- the local `userve` server may not be running
- the remote `Uagent` client may not be connected

`push ...` fails

- the `.efi` path is wrong
- no remote client is connected

`run` produces no useful remote output

- the uploaded EFI may only be using `Print()`
- the app may not be locating `UAGENT_DEBUG_PROTOCOL`
- the remote session may no longer be active
- the app may be sending remote text, but not with a recognizable start or completion marker
- the payload may not be readable text

## Agent Decision Rules

When the user asks for remote-visible text:

- use `SendDebugMessage()`

When the user asks for local operator feedback:

- use `Print()`

When the user asks for both:

- use both, for different audiences

When the user asks to "make an EDK app that prints to the server":

- create a UEFI application in this package
- consume `UAGENT_DEBUG_PROTOCOL`
- send the text through `SendDebugMessage()`
- include a distinctive remote start or completion marker
- make sure `.inf` and `.dsc` are correct

When the user asks to deploy or run the built EFI remotely:

- run `./bin/userver`
- use `push /full/path/to/file.efi`
- use `run`
- confirm that the expected remote marker text actually appeared on the server

## Default Success Criteria

A change in this package is successful when:

- the module builds as a UEFI application
- it can locate `UAGENT_DEBUG_PROTOCOL`
- it calls `SendDebugMessage()` successfully
- the message becomes visible on the Go server
- the agent can deploy and run the resulting `.efi` through `userve` when the task requires remote execution
- the remote output is distinctive enough for an operator to verify that the correct EFI actually ran
