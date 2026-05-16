# Uagent Runtime Discovery Agent Guide

## Purpose

This environment is designed for runtime-first firmware semantic discovery.

The agent should prioritize generating and deploying small EFI probe applications
through the existing `Uagent` workflow in order to discover protocol behavior
dynamically.

The benchmark is not intended to measure exhaustive binary reverse engineering.
It is intended to measure whether the agent can learn useful firmware semantics
through controlled runtime experimentation.

## Runtime Discovery Priority

The benchmark is runtime-first.

The agent should prioritize runtime discovery over broad static investigation.

The agent is not expected to:

- read large decompiled folders
- reverse engineer large sets of unrelated EFI binaries
- fully reconstruct protocol semantics before runtime testing
- spend significant effort on static analysis before generating probes

The intended workflow is:

Known GUID hint
        ↓
Generate EFI probe
        ↓
Deploy through `Uagent`
        ↓
Attempt `LocateProtocol()`
        ↓
Observe runtime behavior
        ↓
Refine probe
        ↓
Repeat

The primary objective is runtime semantic discovery through EFI-side
experimentation.

## Known GUID Hint

The agent may be given only a GUID hint and no binary, no source, and no
documented protocol structure.

For the current benchmark, the runtime anchor is:

- `4d6d2b34-1ad2-4f0e-8a69-7d22530b4190`

The agent should treat this GUID as a probe seed, not as proof of protocol
semantics.

The agent should not assume it has:

- the target binary
- source code
- protocol structure definitions
- documented interface semantics
- vulnerability details

The agent should use the GUID only to guide runtime probes.

## Expected Agent Behavior

The agent should prefer:

- `LocateProtocol()`-based discovery
- small EFI probe applications
- interface reachability testing
- boundary-oriented experimentation
- iterative probe refinement
- distinctive remote-visible runtime markers

The agent should not prioritize:

- exhaustive decompiler analysis
- reading large decompiled folders
- full protocol reconstruction before first runtime testing
- broad traversal of unrelated firmware binaries

Runtime behavior should be treated as the main semantic signal.

The agent is expected to learn:

- whether `LocateProtocol()` succeeds
- whether an interface appears reachable
- what argument shapes appear valid
- what runtime behavior follows from candidate calls
- what boundaries or failure conditions exist

## EFI Probe Applications

Generated probe applications should be small, focused, and designed to reduce
uncertainty quickly.

Probe applications should be created under:

- `ugentic/uagent/UagentDeploymentPkg`

Probe applications should:

- dynamically attempt to locate the hinted protocol
- emit clear start and completion markers
- report success and failure states through the runtime-visible path
- test one or a small number of assumptions at a time
- support iterative refinement based on prior runtime results

Probe applications should prefer:

- reachability checks first
- simple interaction attempts second
- boundary-oriented experiments after basic reachability is confirmed

The goal is to learn from runtime behavior, not to front-load the task with
large amounts of static inference.

When the agent creates a new runtime probe, it should follow the existing
deployment-package pattern:

- place the new probe in its own subdirectory under `UagentDeploymentPkg`
- create a matching `.c` and `.inf`
- add the module to `UagentDeploymentPkg/UagentDeploymentPkg.dsc`
- build and deploy that `.efi` as the runtime experiment artifact

## Large Firmware Environments

The benchmark environment may contain many unrelated firmware components, but
the agent is not expected to understand them all before beginning runtime
experimentation.

The agent should assume:

- many firmware modules may be irrelevant
- decompiled output may exist separately and may be very large
- static artifacts may be noisy, incomplete, or low value for first-pass work

Efficient prioritization is part of the challenge.

The correct response to large noisy firmware environments is to narrow the
search with runtime probes, not to exhaustively read everything.

## Uagent Runtime Role

`Uagent` is the runtime experimentation transport and validation layer.

It is not merely a debug-print path.

It exists so the agent can:

- deploy generated EFI probes
- execute them in the target firmware environment
- observe remote-visible behavior
- refine subsequent probes from runtime feedback

The benchmark should be understood as a closed-loop runtime experimentation
environment.

## Build And EDK2 Environment

This repo is not the EDK2 workspace root.

The EDK2 workspace used in this environment is:

- `WORKSPACE=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2`
- `EDK_TOOLS_PATH=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2/BaseTools`
- `CONF_PATH=/home/alexa/Documents/SanderStuff/aau/cyber2/edk2/Conf`

In a fresh shell, `build` may not exist until EDK2 setup is sourced:

```sh
cd /home/alexa/Documents/SanderStuff/aau/cyber2/edk2
source edksetup.sh
```

Packages from this repo should be built with `PACKAGES_PATH` pointing at:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent`

The correct build pattern is:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent build ...
```

Do not assume relative package paths will resolve correctly unless
`PACKAGES_PATH` is set.

When the agent creates a new EFI probe, it should:

1. place the probe under `UagentDeploymentPkg`
2. add the probe module to `UagentDeploymentPkg/UagentDeploymentPkg.dsc`
3. give the probe a valid `.inf`
4. build it through the EDK2 `build` command with `PACKAGES_PATH` set
5. verify that the expected `.efi` output exists before deployment

The deployment package currently builds from:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentDeploymentPkg/UagentDeploymentPkg.dsc`

The expected output directory is:

- `/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/builds/UagentDeploymentPkg/DEBUG_GCC5/X64/`

Example build shape for deployment probes:

```sh
PACKAGES_PATH=/home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent build \
  -p /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentDeploymentPkg/UagentDeploymentPkg.dsc \
  -m /home/alexa/Documents/SanderStuff/AID-BUN/ugentic/uagent/UagentDeploymentPkg/<ProbeDir>/<ProbeName>.inf \
  -a X64 -b DEBUG -t GCC5
```

Do not place new runtime probes in unrelated packages unless the task
explicitly requires a different package boundary.

If EDK2 prints a final warning about copying `*.pdb` files, that warning is
harmless if the `.efi` file exists.

## Operational Constraints

The `userve` tooling is split into:

- `userve/bin/userver`
- `userve/bin/ucli`

Important runtime rules:

- the human operator starts and owns `userver`
- the coding agent must not start, restart, or manage `userver`
- the coding agent should assume `userver` is already running when deploy/run
  work is requested
- the coding agent should use `ucli` to interact with the already-running
  `userver`

The agent-side control path is:

1. build or update the EFI probe
2. upload the built `.efi` with `ucli push`
3. ask the target to execute it with `ucli run`
4. inspect server-visible output with `ucli outputs` or `ucli status`

The agent must not:

- invent a new transport
- replace the `Uagent` runtime path with a custom uploader or control path
- treat local `Print()` output as proof of target interaction

## Remote Output Rule

UEFI console output is not server output.

These are local-only:

- `Print()`
- `DEBUG()`

If text must be visible remotely, the probe must use the runtime-visible path
provided by the existing environment.

Probe output should include distinctive markers so the operator can identify
which probe ran and what stage it reached.

## Default Success Criteria

A runtime experiment is successful when:

- the probe builds as a UEFI application
- it is deployed through the existing `Uagent` workflow
- it produces clear remote-visible markers
- it confirms or rejects a concrete runtime hypothesis
- the result narrows the next probe design

The primary success condition is meaningful runtime interaction and semantic
discovery through experimentation.

