#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include "../../UagentPkg/Uagent.h"

#define VARIABLE_NAME_CAPACITY   1024
#define REMOTE_LINE_CAPACITY     512
STATIC EFI_GUID  mUagentDebugProtocolGuid = UAGENT_DEBUG_PROTOCOL_GUID;

STATIC
EFI_STATUS
SendLine (
  IN UAGENT_DEBUG_PROTOCOL  *Debug,
  IN CONST CHAR16           *Line
  )
{
  if ((Debug == NULL) || (Line == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return Debug->SendDebugMessage (Debug, Line);
}

STATIC
EFI_STATUS
SendVariableSummary (
  IN UAGENT_DEBUG_PROTOCOL  *Debug,
  IN UINTN                  VariableIndex,
  IN CONST CHAR16           *VariableName,
  IN CONST EFI_GUID         *VendorGuid
  )
{
  CHAR16  Buffer[REMOTE_LINE_CAPACITY];

  UnicodeSPrint (
    Buffer,
    sizeof (Buffer),
    L"Var %u %s %g",
    (UINT32)VariableIndex,
    VariableName,
    VendorGuid
    );
  return SendLine (Debug, Buffer);
}

STATIC
EFI_STATUS
DumpAllVariables (
  IN UAGENT_DEBUG_PROTOCOL  *Debug
  )
{
  EFI_STATUS  Status;
  CHAR16      *VariableName;
  UINTN       VariableNameSize;
  EFI_GUID    VendorGuid;
  UINTN       VariableIndex;
  CHAR16      Buffer[REMOTE_LINE_CAPACITY];

  VariableName = AllocateZeroPool (VARIABLE_NAME_CAPACITY);
  if (VariableName == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  VariableIndex    = 0;
  VariableName[0]  = L'\0';
  ZeroMem (&VendorGuid, sizeof (VendorGuid));

  Status = SendLine (Debug, L"[UagentDumpNvramVars] enumeration begin");
  if (EFI_ERROR (Status)) {
    FreePool (VariableName);
    return Status;
  }

  for (;;) {
    VariableNameSize = VARIABLE_NAME_CAPACITY;
    Status           = gRT->GetNextVariableName (
                              &VariableNameSize,
                              VariableName,
                              &VendorGuid
                              );
    if (Status == EFI_NOT_FOUND) {
      break;
    }

    if (Status == EFI_BUFFER_TOO_SMALL) {
      UnicodeSPrint (
        Buffer,
        sizeof (Buffer),
        L"[UagentDumpNvramVars] variable name exceeded %u bytes",
        (UINT32)VARIABLE_NAME_CAPACITY
        );
      SendLine (Debug, Buffer);
      FreePool (VariableName);
      return Status;
    }

    if (EFI_ERROR (Status)) {
      UnicodeSPrint (
        Buffer,
        sizeof (Buffer),
        L"[UagentDumpNvramVars] GetNextVariableName failed: %r",
        Status
        );
      SendLine (Debug, Buffer);
      FreePool (VariableName);
      return Status;
    }

    VariableIndex++;

    Status = SendVariableSummary (
               Debug,
               VariableIndex,
               VariableName,
               &VendorGuid
               );

    if (EFI_ERROR (Status)) {
      FreePool (VariableName);
      return Status;
    }
  }

  UnicodeSPrint (
    Buffer,
    sizeof (Buffer),
    L"[UagentDumpNvramVars] enumeration end count=%u",
    (UINT32)VariableIndex
    );
  Status = SendLine (Debug, Buffer);

  FreePool (VariableName);
  return Status;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS             Status;
  UAGENT_DEBUG_PROTOCOL  *Debug;
  CHAR16                 Buffer[REMOTE_LINE_CAPACITY];

  Status = gBS->LocateProtocol (
                  &mUagentDebugProtocolGuid,
                  NULL,
                  (VOID **)&Debug
                  );
  if (EFI_ERROR (Status)) {
    Print (L"LocateProtocol failed: %r\n", Status);
    return Status;
  }

  Status = SendLine (Debug, L"[UagentDumpNvramVars] start");
  if (EFI_ERROR (Status)) {
    Print (L"Start marker failed: %r\n", Status);
    return Status;
  }

  Status = DumpAllVariables (Debug);
  if (EFI_ERROR (Status)) {
    UnicodeSPrint (
      Buffer,
      sizeof (Buffer),
      L"[UagentDumpNvramVars] end failure: %r",
      Status
      );
    SendLine (Debug, Buffer);
    Print (L"NVRAM dump failed: %r\n", Status);
    return Status;
  }

  Status = SendLine (Debug, L"[UagentDumpNvramVars] end success");
  Print (L"NVRAM dump sent to userve: %r\n", Status);
  return Status;
}
