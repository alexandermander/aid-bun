#include "Uagent.h"

typedef struct {
  EFI_HANDLE            SelectedHandle;
  SOCKET_CONFIG_SOURCE  ConfigSource;
  EFI_IPv4_ADDRESS      StationAddress;
  EFI_IPv4_ADDRESS      SubnetMask;
  EFI_IPv4_ADDRESS      DhcpServerAddress;
  EFI_IPv4_ADDRESS      RemoteAddress;
} NETWORK_STATUS;

STATIC NETWORK_STATUS  mLastNetworkStatus;

STATIC
VOID
EFIAPI
Tcp4TokenNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  (VOID)Event;
  (VOID)Context;
}

STATIC
EFI_STATUS
WaitForTcp4Token (
  IN EFI_TCP4_PROTOCOL          *Tcp4,
  IN EFI_TCP4_COMPLETION_TOKEN  *CompletionToken
  )
{
  if ((Tcp4 == NULL) || (CompletionToken == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  while (CompletionToken->Status == EFI_NOT_READY) {
    Tcp4->Poll (Tcp4);
    gBS->Stall (1000);
  }

  return CompletionToken->Status;
}

STATIC
VOID
UpdateLastNetworkStatus (
  IN EFI_HANDLE                   SelectedHandle,
  IN SOCKET_CONFIG_SOURCE         ConfigSource,
  IN CONST EFI_IPv4_ADDRESS       *StationAddress OPTIONAL,
  IN CONST EFI_IPv4_ADDRESS       *SubnetMask OPTIONAL,
  IN CONST EFI_IPv4_ADDRESS       *DhcpServerAddress OPTIONAL,
  IN CONST EFI_IPv4_ADDRESS       *RemoteAddress OPTIONAL
  )
{
  mLastNetworkStatus.SelectedHandle = SelectedHandle;
  mLastNetworkStatus.ConfigSource   = ConfigSource;

  if (StationAddress != NULL) {
    CopyMem (&mLastNetworkStatus.StationAddress, StationAddress, sizeof (EFI_IPv4_ADDRESS));
  } else {
    ZeroMem (&mLastNetworkStatus.StationAddress, sizeof (EFI_IPv4_ADDRESS));
  }

  if (SubnetMask != NULL) {
    CopyMem (&mLastNetworkStatus.SubnetMask, SubnetMask, sizeof (EFI_IPv4_ADDRESS));
  } else {
    ZeroMem (&mLastNetworkStatus.SubnetMask, sizeof (EFI_IPv4_ADDRESS));
  }

  if (DhcpServerAddress != NULL) {
    CopyMem (&mLastNetworkStatus.DhcpServerAddress, DhcpServerAddress, sizeof (EFI_IPv4_ADDRESS));
  } else {
    ZeroMem (&mLastNetworkStatus.DhcpServerAddress, sizeof (EFI_IPv4_ADDRESS));
  }

  if (RemoteAddress != NULL) {
    CopyMem (&mLastNetworkStatus.RemoteAddress, RemoteAddress, sizeof (EFI_IPv4_ADDRESS));
  } else {
    ZeroMem (&mLastNetworkStatus.RemoteAddress, sizeof (EFI_IPv4_ADDRESS));
  }
}

STATIC
CONST CHAR16 *
GetConfigSourceText (
  IN SOCKET_CONFIG_SOURCE  ConfigSource
  )
{
  switch (ConfigSource) {
    case SocketConfigSourcePxe:
      return L"pxe";
    case SocketConfigSourceIpv4:
      return L"ipv4-config";
    default:
      return L"unknown";
  }
}

STATIC
VOID
PrintIpv4Field (
  IN CONST CHAR16        *Label,
  IN CONST EFI_IPv4_ADDRESS  *Address
  )
{
  if ((Label == NULL) || (Address == NULL)) {
    return;
  }

  if (IsZeroIpv4Address (Address)) {
    Print (L"%s: unavailable\n", Label);
    return;
  }

  Print (
    L"%s: %d.%d.%d.%d\n",
    Label,
    Address->Addr[0],
    Address->Addr[1],
    Address->Addr[2],
    Address->Addr[3]
    );
}

STATIC
EFI_STATUS
DeriveRemoteAddress (
  IN  CONST EFI_IPv4_ADDRESS  *StationAddress,
  OUT EFI_IPv4_ADDRESS        *RemoteAddress
  )
{
  if ((StationAddress == NULL) || (RemoteAddress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if (IsZeroIpv4Address (StationAddress)) {
    ZeroMem (RemoteAddress, sizeof (*RemoteAddress));
    return EFI_NOT_FOUND;
  }

  CopyMem (RemoteAddress, StationAddress, sizeof (*RemoteAddress));
  RemoteAddress->Addr[3] = 1;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
GetTcp4Candidates (
  IN  EFI_HANDLE  ImageHandle,
  OUT EFI_HANDLE  **HandleBuffer,
  OUT UINTN       *HandleCount
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                *Tcp4Handles;
  EFI_IP4_CONFIG2_PROTOCOL  *Ip4Config2;
  EFI_HANDLE                *Candidates;
  UINTN                     Tcp4HandleCount;
  UINTN                     Index;
  UINTN                     CandidateCount;

  if ((HandleBuffer == NULL) || (HandleCount == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *HandleBuffer = NULL;
  *HandleCount  = 0;
  Tcp4Handles   = NULL;
  Candidates    = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiTcp4ServiceBindingProtocolGuid,
                  NULL,
                  &Tcp4HandleCount,
                  &Tcp4Handles
                  );
  if (EFI_ERROR (Status) || (Tcp4HandleCount == 0)) {
    return EFI_NOT_FOUND;
  }

  Candidates = AllocateZeroPool (sizeof (EFI_HANDLE) * Tcp4HandleCount);
  if (Candidates == NULL) {
    FreePool (Tcp4Handles);
    return EFI_OUT_OF_RESOURCES;
  }

  CandidateCount = 0;
  for (Index = 0; Index < Tcp4HandleCount; Index++) {
    Status = gBS->OpenProtocol (
                    Tcp4Handles[Index],
                    &gEfiIp4Config2ProtocolGuid,
                    (VOID **)&Ip4Config2,
                    ImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status) || (Ip4Config2 == NULL)) {
      continue;
    }

    Candidates[CandidateCount++] = Tcp4Handles[Index];
  }

  FreePool (Tcp4Handles);
  if (CandidateCount == 0) {
    FreePool (Candidates);
    return EFI_NOT_FOUND;
  }

  *HandleBuffer = Candidates;
  *HandleCount  = CandidateCount;
  return EFI_SUCCESS;
}


STATIC
EFI_STATUS
ConfigureTcp4Client (
  IN  EFI_HANDLE            ImageHandle,
  IN  EFI_HANDLE            ControllerHandle,
  IN  SOCKET_CONFIG_SOURCE  ConfigSource,
  IN  EFI_IPv4_ADDRESS      *StationAddress,
  IN  EFI_IPv4_ADDRESS      *SubnetMask,
  IN  EFI_IPv4_ADDRESS      *DhcpServerAddress,
  OUT SOCKET_CLIENT         *Client
  )
{
  EFI_STATUS            Status;
  EFI_TCP4_CONFIG_DATA  ConfigData;
  EFI_IPv4_ADDRESS      RemoteAddress;

  if ((StationAddress == NULL) || (SubnetMask == NULL) || (DhcpServerAddress == NULL) || (Client == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Client, sizeof (*Client));
  Client->ServiceHandle = ControllerHandle;
  Client->SelectedHandle = ControllerHandle;
  Client->ConfigSource = ConfigSource;
  CopyMem (&Client->StationAddress, StationAddress, sizeof (Client->StationAddress));
  CopyMem (&Client->SubnetMask, SubnetMask, sizeof (Client->SubnetMask));
  CopyMem (&Client->DhcpServerAddress, DhcpServerAddress, sizeof (Client->DhcpServerAddress));
  Status = DeriveRemoteAddress (StationAddress, &Client->RemoteAddress);
  if (EFI_ERROR (Status)) {
    Print (L"InitSocketClient: Failed to derive remote IPv4 from local address\n");
    goto Error;
  }

  Status = gBS->OpenProtocol (
                  Client->ServiceHandle,
                  &gEfiTcp4ServiceBindingProtocolGuid,
                  (VOID **)&Client->ServiceBinding,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    Print (L"InitSocketClient: Open TCP4 service binding failed: %r\n", Status);
    goto Error;
  }

  Status = Client->ServiceBinding->CreateChild (Client->ServiceBinding, &Client->ChildHandle);
  if (EFI_ERROR (Status)) {
    Print (L"InitSocketClient: TCP4 CreateChild failed: %r\n", Status);
    goto Error;
  }

  Status = gBS->OpenProtocol (
                  Client->ChildHandle,
                  &gEfiTcp4ProtocolGuid,
                  (VOID **)&Client->Tcp4,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    Print (L"InitSocketClient: Open TCP4 protocol failed: %r\n", Status);
    goto Error;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Tcp4TokenNotify, NULL, &Client->ConnectEvent);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Tcp4TokenNotify, NULL, &Client->TransmitEvent);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Tcp4TokenNotify, NULL, &Client->ReceiveEvent);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  Status = gBS->CreateEvent (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Tcp4TokenNotify, NULL, &Client->CloseEvent);
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  ZeroMem (&ConfigData, sizeof (ConfigData));
  CopyMem (&RemoteAddress, &Client->RemoteAddress, sizeof (RemoteAddress));

  ConfigData.TypeOfService                 = 0;
  ConfigData.TimeToLive                    = 64;
  ConfigData.AccessPoint.UseDefaultAddress = (BOOLEAN)(ConfigSource == SocketConfigSourceIpv4);
  ConfigData.AccessPoint.ActiveFlag        = TRUE;
  ConfigData.AccessPoint.StationPort       = 0;
  ConfigData.AccessPoint.RemotePort        = REMOTE_PORT;
  if (!ConfigData.AccessPoint.UseDefaultAddress) {
    CopyMem (&ConfigData.AccessPoint.StationAddress, StationAddress, sizeof (EFI_IPv4_ADDRESS));
    CopyMem (&ConfigData.AccessPoint.SubnetMask, SubnetMask, sizeof (EFI_IPv4_ADDRESS));
  }

  CopyMem (&ConfigData.AccessPoint.RemoteAddress, &RemoteAddress, sizeof (EFI_IPv4_ADDRESS));

  Status = Client->Tcp4->Configure (Client->Tcp4, &ConfigData);
  if (EFI_ERROR (Status)) {
    Print (L"InitSocketClient: TCP4 configure failed: %r\n", Status);
    goto Error;
  }

  ZeroMem (&Client->ConnectToken, sizeof (Client->ConnectToken));
  Client->ConnectToken.CompletionToken.Event  = Client->ConnectEvent;
  Client->ConnectToken.CompletionToken.Status = EFI_NOT_READY;

  Status = Client->Tcp4->Connect (Client->Tcp4, &Client->ConnectToken);
  if (EFI_ERROR (Status)) {
    if (ConfigSource == SocketConfigSourceIpv4) {
      Print (L"TCP4 connect failed after IPv4 config\n");
    }

    Print (L"InitSocketClient: TCP4 connect start failed: %r\n", Status);
    goto Error;
  }

  Status = WaitForTcp4Token (Client->Tcp4, &Client->ConnectToken.CompletionToken);
  if (!EFI_ERROR (Status)) {
    UpdateLastNetworkStatus (
      ControllerHandle,
      ConfigSource,
      StationAddress,
      SubnetMask,
      DhcpServerAddress,
      &Client->RemoteAddress
      );
    Print (
      L"Connected to server %d.%d.%d.%d:%d\n",
      RemoteAddress.Addr[0],
      RemoteAddress.Addr[1],
      RemoteAddress.Addr[2],
      RemoteAddress.Addr[3],
      REMOTE_PORT
      );
    return Status;
  }

  if (ConfigSource == SocketConfigSourceIpv4) {
    Print (L"TCP4 connect failed after IPv4 config\n");
  }

  Print (L"InitSocketClient: TCP4 connect completion failed: %r\n", Status);

Error:
  CloseSocketClient (Client);
  return Status;
}

EFI_STATUS
InitSocketClient (
  IN  EFI_HANDLE     ImageHandle,
  OUT SOCKET_CLIENT  *Client
  )
{
  EFI_STATUS        Status;
  EFI_STATUS        LastError;
  EFI_HANDLE        ControllerHandle;
  EFI_HANDLE        *CandidateHandles;
  UINTN             CandidateCount;
  UINTN             CandidateIndex;
  EFI_IPv4_ADDRESS  StationAddress;
  EFI_IPv4_ADDRESS  SubnetMask;
  EFI_IPv4_ADDRESS  DhcpServerAddress;

  if (Client == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Client, sizeof (*Client));
  ControllerHandle = NULL;
  CandidateHandles = NULL;
  CandidateCount   = 0;
  LastError        = EFI_NOT_FOUND;
  ZeroMem (&StationAddress, sizeof (StationAddress));
  ZeroMem (&SubnetMask, sizeof (SubnetMask));
  ZeroMem (&DhcpServerAddress, sizeof (DhcpServerAddress));
  // setting alle to the right sise
  UpdateLastNetworkStatus (NULL, SocketConfigSourceUnknown, NULL, NULL, NULL, NULL);

  Status = TryGetPxeConfig (ImageHandle, &ControllerHandle, &StationAddress, &SubnetMask, &DhcpServerAddress);
  if (!EFI_ERROR (Status)) {
    return ConfigureTcp4Client (
             ImageHandle,
             ControllerHandle,
             SocketConfigSourcePxe,
             &StationAddress,
             &SubnetMask,
             &DhcpServerAddress,
             Client
             );
  }

  Print (L"PXE config unavailable\n");

  Status = GetTcp4Candidates (ImageHandle, &CandidateHandles, &CandidateCount);
  if (EFI_ERROR (Status)) {
    Print (L"No usable NIC with IP4Config2 + TCP4\n");
    return Status;
  }

  for (CandidateIndex = 0; CandidateIndex < CandidateCount; CandidateIndex++) {
    ControllerHandle = CandidateHandles[CandidateIndex];
    UpdateLastNetworkStatus (ControllerHandle, SocketConfigSourceIpv4, NULL, NULL, NULL, NULL);
    Status           = AcquireIpv4Config (
                         ImageHandle,
                         ControllerHandle,
                         &StationAddress,
                         &SubnetMask,
                         &DhcpServerAddress
                         );
    if (EFI_ERROR (Status)) {
      LastError = Status;
      continue;
    }

    Status = ConfigureTcp4Client (
               ImageHandle,
               ControllerHandle,
               SocketConfigSourceIpv4,
               &StationAddress,
               &SubnetMask,
               &DhcpServerAddress,
               Client
               );
    if (!EFI_ERROR (Status)) {
      FreePool (CandidateHandles);
      return EFI_SUCCESS;
    }

    LastError = Status;
  }

  FreePool (CandidateHandles);
  return LastError;
}

EFI_STATUS
SendCommandPacket (
  IN SOCKET_CLIENT       *Client,
  IN CONST TCP_COMMAND   *Command
  )
{
  EFI_STATUS              Status;
  EFI_TCP4_TRANSMIT_DATA  *TransmitData;
  UINT8                   *PacketBuffer;
  UINTN                   AllocationSize;

  if ((Client == NULL) || (Client->Tcp4 == NULL) || (Command == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Command->Type == 0) || (Command->Type >= TcpMaxCommand)) {
    return EFI_INVALID_PARAMETER;
  }

  AllocationSize = COMMAND_HEADER_SIZE + Command->PayloadSize;
  PacketBuffer   = AllocateZeroPool (AllocationSize);
  TransmitData   = AllocateZeroPool (sizeof (EFI_TCP4_TRANSMIT_DATA));
  if ((PacketBuffer == NULL) || (TransmitData == NULL)) {
    if (PacketBuffer != NULL) {
      FreePool (PacketBuffer);
    }
    if (TransmitData != NULL) {
      FreePool (TransmitData);
    }
    return EFI_OUT_OF_RESOURCES;
  }

  PacketBuffer[0] = (UINT8)Command->Type;
  PacketBuffer[1] = (UINT8)(Command->PayloadSize & 0xFF);
  PacketBuffer[2] = (UINT8)((Command->PayloadSize >> 8) & 0xFF);
  if (Command->PayloadSize > 0 && Command->Payload != NULL) {
    CopyMem (PacketBuffer + COMMAND_HEADER_SIZE, Command->Payload, Command->PayloadSize);
  }

  TransmitData->Push                            = TRUE;
  TransmitData->Urgent                          = FALSE;
  TransmitData->DataLength                      = (UINT32)AllocationSize;
  TransmitData->FragmentCount                   = 1;
  TransmitData->FragmentTable[0].FragmentLength = (UINT32)AllocationSize;
  TransmitData->FragmentTable[0].FragmentBuffer = PacketBuffer;

  ZeroMem (&Client->TransmitToken, sizeof (Client->TransmitToken));
  Client->TransmitToken.CompletionToken.Event  = Client->TransmitEvent;
  Client->TransmitToken.CompletionToken.Status = EFI_NOT_READY;
  Client->TransmitToken.Packet.TxData          = TransmitData;

  Status = Client->Tcp4->Transmit (Client->Tcp4, &Client->TransmitToken);
  if (!EFI_ERROR (Status)) {
    Status = WaitForTcp4Token (Client->Tcp4, &Client->TransmitToken.CompletionToken);
  }

  FreePool (PacketBuffer);
  FreePool (TransmitData);
  return Status;
}

STATIC
EFI_STATUS
ReceiveExactBytes (
  IN  SOCKET_CLIENT  *Client,
  OUT UINT8          *Buffer,
  IN  UINTN          BufferSize
  )
{
  EFI_STATUS             Status;
  EFI_TCP4_RECEIVE_DATA  *ReceiveData;
  UINT8                  ChunkBuffer[RESPONSE_CHUNK_SIZE];
  UINTN                  AllocationSize;
  UINTN                  TotalRead;
  UINTN                  CopyLength;
  UINTN                  StashCopyLength;

  if ((Client == NULL) || (Client->Tcp4 == NULL) || (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  AllocationSize = sizeof (EFI_TCP4_RECEIVE_DATA);
  ReceiveData    = AllocateZeroPool (AllocationSize);
  if (ReceiveData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  TotalRead = 0;
  Status    = EFI_SUCCESS;
  while (TotalRead < BufferSize) {
    if (Client->ReceiveStashLength > 0) {
      StashCopyLength = MIN (Client->ReceiveStashLength, BufferSize - TotalRead);
      CopyMem (Buffer + TotalRead, Client->ReceiveStash, StashCopyLength);
      TotalRead += StashCopyLength;

      if (StashCopyLength < Client->ReceiveStashLength) {
        CopyMem (
          Client->ReceiveStash,
          Client->ReceiveStash + StashCopyLength,
          Client->ReceiveStashLength - StashCopyLength
          );
      }

      Client->ReceiveStashLength -= StashCopyLength;
      continue;
    }

    ZeroMem (ReceiveData, AllocationSize);
    ZeroMem (ChunkBuffer, sizeof (ChunkBuffer));

    ReceiveData->UrgentFlag                      = FALSE;
    ReceiveData->DataLength                      = sizeof (ChunkBuffer);
    ReceiveData->FragmentCount                   = 1;
    ReceiveData->FragmentTable[0].FragmentLength = sizeof (ChunkBuffer);
    ReceiveData->FragmentTable[0].FragmentBuffer = ChunkBuffer;

    ZeroMem (&Client->ReceiveToken, sizeof (Client->ReceiveToken));
    Client->ReceiveToken.CompletionToken.Event  = Client->ReceiveEvent;
    Client->ReceiveToken.CompletionToken.Status = EFI_NOT_READY;
    Client->ReceiveToken.Packet.RxData          = ReceiveData;

    Status = Client->Tcp4->Receive (Client->Tcp4, &Client->ReceiveToken);
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = WaitForTcp4Token (Client->Tcp4, &Client->ReceiveToken.CompletionToken);
    if (Status == EFI_CONNECTION_FIN) {
      if (TotalRead == 0) {
        break;
      }

      Status = EFI_DEVICE_ERROR;
      break;
    }

    if (EFI_ERROR (Status)) {
      break;
    }

    if (ReceiveData->DataLength == 0) {
      Status = EFI_DEVICE_ERROR;
      break;
    }

    CopyLength = MIN ((UINTN)ReceiveData->DataLength, BufferSize - TotalRead);
    CopyMem (Buffer + TotalRead, ChunkBuffer, CopyLength);
    TotalRead += CopyLength;

    if ((UINTN)ReceiveData->DataLength > CopyLength) {
      Client->ReceiveStashLength = (UINTN)ReceiveData->DataLength - CopyLength;
      CopyMem (
        Client->ReceiveStash,
        ChunkBuffer + CopyLength,
        Client->ReceiveStashLength
        );
    }
  }

  FreePool (ReceiveData);
  return Status;
}

EFI_STATUS
ReceiveCommandPacket (
  IN  SOCKET_CLIENT  *Client,
  OUT TCP_COMMAND    *Command
  )
{
  EFI_STATUS  Status;
  UINT8       Header[COMMAND_HEADER_SIZE];
  UINT16      PayloadLength;

  if ((Client == NULL) || (Command == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Command, sizeof (*Command));

  Status = ReceiveExactBytes (Client, Header, sizeof (Header));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Command->Type = (COMMAND_TYPE)Header[0];
  PayloadLength = (UINT16)(Header[1] | (Header[2] << 8));
  Command->PayloadSize = PayloadLength;

  if (PayloadLength == 0) {
    return EFI_SUCCESS;
  }

  Command->Payload = AllocateZeroPool ((UINTN)PayloadLength + 1); // +1 for null termination safety
  if (Command->Payload == NULL) {
    Print (L"Failed to allocate memory for command payload\n");
    return EFI_OUT_OF_RESOURCES;
  }

  Status = ReceiveExactBytes (Client, (UINT8 *)Command->Payload, PayloadLength);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to receive payload data\n");
    FreeCommandPacket (Command);
    return Status;
  }

  return EFI_SUCCESS;
}

VOID
FreeCommandPacket (
  IN OUT TCP_COMMAND  *Command
  )
{
  if (Command == NULL) {
    return;
  }

  if (Command->Payload != NULL) {
    FreePool (Command->Payload);
    Command->Payload = NULL;
  }

  Command->PayloadSize = 0;
}

VOID
CloseSocketClient (
  IN SOCKET_CLIENT  *Client
  )
{
  if (Client == NULL) {
    return;
  }

  if (Client->Tcp4 != NULL) {
    ZeroMem (&Client->CloseToken, sizeof (Client->CloseToken));
    Client->CloseToken.CompletionToken.Event  = Client->CloseEvent;
    Client->CloseToken.CompletionToken.Status = EFI_NOT_READY;

    if (!EFI_ERROR (Client->Tcp4->Close (Client->Tcp4, &Client->CloseToken))) {
      WaitForTcp4Token (Client->Tcp4, &Client->CloseToken.CompletionToken);
    }

    Client->Tcp4->Configure (Client->Tcp4, NULL);
  }

  if ((Client->ChildHandle != NULL) && (Client->ServiceBinding != NULL)) {
    Client->ServiceBinding->DestroyChild (Client->ServiceBinding, Client->ChildHandle);
  }

  if (Client->ConnectEvent != NULL) {
    gBS->CloseEvent (Client->ConnectEvent);
  }

  if (Client->TransmitEvent != NULL) {
    gBS->CloseEvent (Client->TransmitEvent);
  }

  if (Client->ReceiveEvent != NULL) {
    gBS->CloseEvent (Client->ReceiveEvent);
  }

  if (Client->CloseEvent != NULL) {
    gBS->CloseEvent (Client->CloseEvent);
  }

  ZeroMem (Client, sizeof (*Client));
}

VOID
PrintLastNetworkStatus (
  VOID
  )
{
  Print (L"Last network attempt:\n");
  Print (L"  source: %s\n", GetConfigSourceText (mLastNetworkStatus.ConfigSource));
  Print (L"  handle: %p\n", mLastNetworkStatus.SelectedHandle);
  PrintIpv4Field (L"  local IPv4", &mLastNetworkStatus.StationAddress);
  PrintIpv4Field (L"  subnet mask", &mLastNetworkStatus.SubnetMask);
  PrintIpv4Field (L"  DHCP server", &mLastNetworkStatus.DhcpServerAddress);
  PrintIpv4Field (L"  remote server", &mLastNetworkStatus.RemoteAddress);
}

VOID
PrintIpStatus (
  VOID
  )
{
  PrintIpv4Field (L"IPv4", &mLastNetworkStatus.StationAddress);
  PrintIpv4Field (L"DHCP server", &mLastNetworkStatus.DhcpServerAddress);
  PrintIpv4Field (L"Remote server", &mLastNetworkStatus.RemoteAddress);
}
