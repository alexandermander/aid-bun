#include <Uefi.h>

#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include "../../UagentPkg/Uagent.h"

STATIC EFI_GUID  mUagentDebugProtocolGuid = UAGENT_DEBUG_PROTOCOL_GUID;

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS             Status;
  UAGENT_DEBUG_PROTOCOL  *Debug;

  Status = gBS->LocateProtocol (
                  &mUagentDebugProtocolGuid,
                  NULL,
                  (VOID **)&Debug
                  );
  if (EFI_ERROR (Status)) {
    Print (L"LocateProtocol failed: %r\r\n", Status);
    return Status;
  }

  Status = Debug->SendDebugMessage (
                    Debug,
                    L"[HelloWorld] start"
                    );
  if (EFI_ERROR (Status)) {
    Print (L"SendDebugMessage start failed: %r\r\n", Status);
    return Status;
  }

  Status = Debug->SendDebugMessage (
                    Debug,
                    L"Hello, World from TestDevPkg!"
                    );
  if (EFI_ERROR (Status)) {
    Print (L"SendDebugMessage hello failed: %r\r\n", Status);
    return Status;
  }

  Status = Debug->SendDebugMessage (
                    Debug,
                    L"[HelloWorld] success"
                    );
  Print (L"SendDebugMessage returned: %r\r\n", Status);
  return Status;
}
