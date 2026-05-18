#include "Uagent.h"

BOOLEAN
IsZeroIpv4Address (
  IN CONST EFI_IPv4_ADDRESS  *Address
  )
{
  if (Address == NULL) {
    return TRUE;
  }

  return (BOOLEAN)(
           (Address->Addr[0] == 0) &&
           (Address->Addr[1] == 0) &&
           (Address->Addr[2] == 0) &&
           (Address->Addr[3] == 0)
           );
}

EFI_STATUS
TryGetPxeConfig (
  IN  EFI_HANDLE        ImageHandle,
  OUT EFI_HANDLE        *ControllerHandle,
  OUT EFI_IPv4_ADDRESS  *StationAddress,
  OUT EFI_IPv4_ADDRESS  *SubnetMask,
  OUT EFI_IPv4_ADDRESS  *DhcpServerAddress
  )
{
  EFI_STATUS                  Status;
  EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;
  EFI_PXE_BASE_CODE_PROTOCOL  *PxeBaseCode;
  EFI_HANDLE                  *HandleBuffer;
  UINTN                       HandleCount;
  UINTN                       HandleIndex;

  if ((ControllerHandle == NULL) || (StationAddress == NULL) || (SubnetMask == NULL) || (DhcpServerAddress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *ControllerHandle = NULL;
  HandleBuffer      = NULL;
  HandleCount       = 0;
  ZeroMem (StationAddress, sizeof (*StationAddress));
  ZeroMem (SubnetMask, sizeof (*SubnetMask));
  ZeroMem (DhcpServerAddress, sizeof (*DhcpServerAddress));

  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status) && (LoadedImage->DeviceHandle != NULL)) {
    Status = gBS->OpenProtocol (
                    LoadedImage->DeviceHandle,
                    &gEfiPxeBaseCodeProtocolGuid,
                    (VOID **)&PxeBaseCode,
                    ImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (!EFI_ERROR (Status) &&
        (PxeBaseCode->Mode != NULL) &&
        PxeBaseCode->Mode->Started &&
        !PxeBaseCode->Mode->UsingIpv6 &&
        PxeBaseCode->Mode->DhcpAckReceived)
    {
      *ControllerHandle = LoadedImage->DeviceHandle;
      CopyMem (StationAddress, &PxeBaseCode->Mode->StationIp.v4, sizeof (*StationAddress));
      CopyMem (SubnetMask, &PxeBaseCode->Mode->SubnetMask.v4, sizeof (*SubnetMask));
      CopyMem (DhcpServerAddress, &PxeBaseCode->Mode->DhcpAck.Dhcpv4.BootpSiAddr, sizeof (*DhcpServerAddress));

      return EFI_SUCCESS;
    }
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPxeBaseCodeProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    return EFI_NOT_FOUND;
  }

  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    Status = gBS->OpenProtocol (
                    HandleBuffer[HandleIndex],
                    &gEfiPxeBaseCodeProtocolGuid,
                    (VOID **)&PxeBaseCode,
                    ImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status) || (PxeBaseCode->Mode == NULL)) {
      continue;
    }

    if (PxeBaseCode->Mode->Started &&
        !PxeBaseCode->Mode->UsingIpv6 &&
        PxeBaseCode->Mode->DhcpAckReceived)
    {
      *ControllerHandle = HandleBuffer[HandleIndex];
      CopyMem (StationAddress, &PxeBaseCode->Mode->StationIp.v4, sizeof (*StationAddress));
      CopyMem (SubnetMask, &PxeBaseCode->Mode->SubnetMask.v4, sizeof (*SubnetMask));
      CopyMem (DhcpServerAddress, &PxeBaseCode->Mode->DhcpAck.Dhcpv4.BootpSiAddr, sizeof (*DhcpServerAddress));

      Status = EFI_SUCCESS;
      break;
    }

    Status = EFI_NOT_FOUND;
  }

  FreePool (HandleBuffer);
  return Status;
}

STATIC
EFI_STATUS
ReadInterfaceInfo (
  IN  EFI_IP4_CONFIG2_PROTOCOL        *Ip4Config2,
  OUT EFI_IP4_CONFIG2_INTERFACE_INFO  **InterfaceInfo
  )
{
  EFI_STATUS                     Status;
  EFI_IP4_CONFIG2_INTERFACE_INFO *Data;
  UINTN                          DataSize;

  if ((Ip4Config2 == NULL) || (InterfaceInfo == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *InterfaceInfo = NULL;
  DataSize       = 0;
  Status         = Ip4Config2->GetData (
                                 Ip4Config2,
                                 Ip4Config2DataTypeInterfaceInfo,
                                 &DataSize,
                                 NULL
                                 );
  if (Status == EFI_NOT_READY) {
    return Status;
  }

  if ((Status != EFI_BUFFER_TOO_SMALL) || (DataSize == 0)) {
    return Status;
  }

  Data = AllocateZeroPool (DataSize);
  if (Data == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = Ip4Config2->GetData (
                         Ip4Config2,
                         Ip4Config2DataTypeInterfaceInfo,
                         &DataSize,
                         Data
                         );
  if (EFI_ERROR (Status)) {
    FreePool (Data);
    return Status;
  }

  *InterfaceInfo = Data;
  return EFI_SUCCESS;
}

EFI_STATUS
AcquireIpv4Config (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_HANDLE        ControllerHandle,
  OUT EFI_IPv4_ADDRESS  *StationAddress,
  OUT EFI_IPv4_ADDRESS  *SubnetMask,
  OUT EFI_IPv4_ADDRESS  *DhcpServerAddress
  )
{
  EFI_STATUS                      Status;
  EFI_IP4_CONFIG2_PROTOCOL        *Ip4Config2;
  EFI_IP4_CONFIG2_INTERFACE_INFO  *InterfaceInfo;

  if ((StationAddress == NULL) || (SubnetMask == NULL) || (DhcpServerAddress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (StationAddress, sizeof (*StationAddress));
  ZeroMem (SubnetMask, sizeof (*SubnetMask));
  ZeroMem (DhcpServerAddress, sizeof (*DhcpServerAddress));
  InterfaceInfo = NULL;

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiIp4Config2ProtocolGuid,
                  (VOID **)&Ip4Config2,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ReadInterfaceInfo (Ip4Config2, &InterfaceInfo);
  if (!EFI_ERROR (Status) && (InterfaceInfo != NULL)) {
    if (!IsZeroIpv4Address (&InterfaceInfo->StationAddress) &&
        !IsZeroIpv4Address (&InterfaceInfo->SubnetMask))
    {
      CopyMem (StationAddress, &InterfaceInfo->StationAddress, sizeof (*StationAddress));
      CopyMem (SubnetMask, &InterfaceInfo->SubnetMask, sizeof (*SubnetMask));
      Print (L"Using existing IPv4 config\n");
      FreePool (InterfaceInfo);
      return EFI_SUCCESS;
    }

    FreePool (InterfaceInfo);
    InterfaceInfo = NULL;
  } else if ((Status != EFI_NOT_READY) && (Status != EFI_NOT_FOUND)) {
    return Status;
  }

  return EFI_NOT_FOUND;
}
