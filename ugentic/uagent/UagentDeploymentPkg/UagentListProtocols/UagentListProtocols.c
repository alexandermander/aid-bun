#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>

#include "../../UagentPkg/Uagent.h"

STATIC EFI_GUID  mUagentDebugProtocolGuid = UAGENT_DEBUG_PROTOCOL_GUID;

typedef struct {
  EFI_GUID    *Guid;
  CONST CHAR16 *Name;
} GUID_NAME_MAPPING;

STATIC GUID_NAME_MAPPING mGuidNameMapping[] = {
  { &gEfiLoadedImageProtocolGuid, L"LoadedImage" },
  { &gEfiPxeBaseCodeProtocolGuid, L"PxeBaseCode" },
  { &gEfiTcp4ProtocolGuid, L"Tcp4" },
  { &gEfiTcp4ServiceBindingProtocolGuid, L"Tcp4ServiceBinding" },
  { &gEfiSimpleFileSystemProtocolGuid, L"SimpleFileSystem" },
  { &gEfiDevicePathProtocolGuid, L"DevicePath" },
  { &gEfiBlockIoProtocolGuid, L"BlockIo" },
  { &gEfiGraphicsOutputProtocolGuid, L"GraphicsOutput" },
  { &gEfiSimpleTextInProtocolGuid, L"SimpleTextIn" },
  { &gEfiSimpleTextOutProtocolGuid, L"SimpleTextOut" },
  { &gEfiSimpleNetworkProtocolGuid, L"SimpleNetwork" },
  { &gEfiIp4ProtocolGuid, L"Ip4" },
  { &gEfiIp4ServiceBindingProtocolGuid, L"Ip4ServiceBinding" },
  { &gEfiArpProtocolGuid, L"Arp" },
  { &gEfiArpServiceBindingProtocolGuid, L"ArpServiceBinding" },
  { &gEfiDhcp4ProtocolGuid, L"Dhcp4" },
  { &gEfiDhcp4ServiceBindingProtocolGuid, L"Dhcp4ServiceBinding" },
  { &gEfiUdp4ProtocolGuid, L"Udp4" },
  { &gEfiUdp4ServiceBindingProtocolGuid, L"Udp4ServiceBinding" },
  { &gEfiMtftp4ProtocolGuid, L"Mtftp4" },
  { &gEfiMtftp4ServiceBindingProtocolGuid, L"Mtftp4ServiceBinding" },
  { &mUagentDebugProtocolGuid, L"UAGENT_DEBUG_PROTOCOL" }
};

CONST CHAR16 *
GetGuidName (
  IN EFI_GUID *Guid
  )
{
  UINTN Index;

  for (Index = 0; Index < ARRAY_SIZE (mGuidNameMapping); Index++) {
    if (CompareGuid (mGuidNameMapping[Index].Guid, Guid)) {
      return mGuidNameMapping[Index].Name;
    }
  }

  return NULL;
}

/**
  Check if a GUID is already in the list.
**/
BOOLEAN
IsGuidInList (
  IN EFI_GUID  *GuidList,
  IN UINTN     Count,
  IN EFI_GUID  *Guid
  )
{
  UINTN  Index;

  for (Index = 0; Index < Count; Index++) {
    if (CompareGuid (&GuidList[Index], Guid)) {
      return TRUE;
    }
  }

  return FALSE;
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
  EFI_HANDLE             *HandleBuffer;
  UINTN                  HandleCount;
  UINTN                  HandleIndex;
  EFI_GUID               **ProtocolBuffer;
  UINTN                  ProtocolCount;
  UINTN                  ProtocolIndex;
  EFI_GUID               *UniqueGuids;
  UINTN                  UniqueCount;
  CHAR16                 Message[128];
  CONST CHAR16           *Name;

  Status = gBS->LocateProtocol (
                  &mUagentDebugProtocolGuid,
                  NULL,
                  (VOID **)&Debug
                  );
  if (EFI_ERROR (Status)) {
    Print (L"LocateProtocol failed: %r\n", Status);
    return Status;
  }

  Debug->SendDebugMessage (Debug, L"[UagentListProtocols] start");

  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    Debug->SendDebugMessage (Debug, L"LocateHandleBuffer failed");
    return Status;
  }

  UniqueGuids = AllocateZeroPool (sizeof (EFI_GUID) * 4096);
  UniqueCount = 0;

  if (UniqueGuids == NULL) {
    FreePool (HandleBuffer);
    return EFI_OUT_OF_RESOURCES;
  }

  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    Status = gBS->ProtocolsPerHandle (
                    HandleBuffer[HandleIndex],
                    &ProtocolBuffer,
                    &ProtocolCount
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    for (ProtocolIndex = 0; ProtocolIndex < ProtocolCount; ProtocolIndex++) {
      if (!IsGuidInList (UniqueGuids, UniqueCount, ProtocolBuffer[ProtocolIndex])) {
        if (UniqueCount < 4096) {
          CopyGuid (&UniqueGuids[UniqueCount], ProtocolBuffer[ProtocolIndex]);
          
          Name = GetGuidName (ProtocolBuffer[ProtocolIndex]);
          if (Name != NULL) {
            UnicodeSPrint (Message, sizeof (Message), L"Protocol: %s (%g)", Name, ProtocolBuffer[ProtocolIndex]);
          } else {
            UnicodeSPrint (Message, sizeof (Message), L"Protocol: %g", ProtocolBuffer[ProtocolIndex]);
          }
          Debug->SendDebugMessage (Debug, Message);
          
          UniqueCount++;
        }
      }
    }

    FreePool (ProtocolBuffer);
  }

  FreePool (HandleBuffer);
  FreePool (UniqueGuids);

  UnicodeSPrint (Message, sizeof (Message), L"[UagentListProtocols] end. Found %d unique protocols", UniqueCount);
  Debug->SendDebugMessage (Debug, Message);

  Print (L"Found and printed %d unique protocols to server.\n", UniqueCount);

  return EFI_SUCCESS;
}
