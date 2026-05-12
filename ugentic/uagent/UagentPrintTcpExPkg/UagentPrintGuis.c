#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>

#include "../UagentPkg/Uagent.h"

STATIC EFI_GUID  mUagentDebugProtocolGuid = UAGENT_DEBUG_PROTOCOL_GUID;

VOID
PrintGuidToRemote (
  IN UAGENT_DEBUG_PROTOCOL  *Debug,
  IN CONST CHAR16           *Name,
  IN CONST EFI_GUID         *Guid
  )
{
  CHAR16  Buffer[128];

  UnicodeSPrint (Buffer, sizeof (Buffer), L"%s: %g", Name, Guid);
  Debug->SendDebugMessage (Debug, Buffer);
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

  Status = gBS->LocateProtocol (
                  &mUagentDebugProtocolGuid,
                  NULL,
                  (VOID **)&Debug
                  );
  if (EFI_ERROR (Status)) {
    Print (L"LocateProtocol failed: %r\n", Status);
    return Status;
  }

  Debug->SendDebugMessage (Debug, L"[UagentPrintGuis] start");

  PrintGuidToRemote (Debug, L"UAGENT_DEBUG_PROTOCOL_GUID", &mUagentDebugProtocolGuid);
  PrintGuidToRemote (Debug, L"gEfiLoadedImageProtocolGuid", &gEfiLoadedImageProtocolGuid);
  PrintGuidToRemote (Debug, L"gEfiPxeBaseCodeProtocolGuid", &gEfiPxeBaseCodeProtocolGuid);
  PrintGuidToRemote (Debug, L"gEfiTcp4ProtocolGuid", &gEfiTcp4ProtocolGuid);
  PrintGuidToRemote (Debug, L"gEfiTcp4ServiceBindingProtocolGuid", &gEfiTcp4ServiceBindingProtocolGuid);

  Debug->SendDebugMessage (Debug, L"[UagentPrintGuis] end");

  Print (L"GUIDs printed to server.\n");

  return EFI_SUCCESS;
}
