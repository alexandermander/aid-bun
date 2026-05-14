# UagentDeploymentPkg

`UagentDeploymentPkg` builds a standalone UEFI application that enumerates every
UEFI variable visible through runtime services and sends the dump to `userve`
through the existing `UAGENT_DEBUG_PROTOCOL`.

The application:

- locates the debug protocol installed by `Uagent.efi`
- walks the full NVRAM variable list with `GetNextVariableName()`
- sends each variable name and vendor GUID to the remote server

## Build

From the EDK2 workspace root:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent build \
  -p /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentDeploymentPkg/UagentDeploymentPkg.dsc \
  -m /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentDeploymentPkg/UagentDumpNvramVars/UagentDumpNvramVars.inf \
  -a X64 -b DEBUG -t GCC5
```

Expected output:

`/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/builds/UagentDeploymentPkg/DEBUG_GCC5/X64/UagentDumpNvramVars.efi`

## Run

1. Start `Uagent.efi` and wait for the remote session to connect.
2. Launch `UagentDumpNvramVars.efi` in the same EFI environment.
3. Watch `userve` for the remote output stream:
   - `[UagentDumpNvramVars] start`
   - one `Var ...` line per NVRAM variable name and vendor GUID
   - `[UagentDumpNvramVars] end ...`

## Notes

- The dump is sent over the existing `TcpOutputText` path used by `UagentPrintTcpExPkg`.
- This version enumerates names and vendor GUIDs only; it does not send variable payload bytes.
