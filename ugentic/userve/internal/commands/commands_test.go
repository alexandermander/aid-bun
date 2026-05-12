package commands

import (
	"net"
	"testing"

	"userve/internal/protocol"
	"userve/internal/session"
)

func TestPushBuildsExpectedPacket(t *testing.T) {
	serverConn, clientConn := net.Pipe()
	defer serverConn.Close()
	defer clientConn.Close()

	manager := &session.Manager{}
	manager.Register(serverConn)

	service := Service{Sessions: manager}

	done := make(chan *protocol.Packet, 1)
	go func() {
		packet, _ := protocol.ReadPacket(clientConn)
		done <- packet
	}()

	if _, err := service.Push("app.efi", []byte("DATA")); err != nil {
		t.Fatalf("Push() error = %v", err)
	}

	packet := <-done
	if packet.Command != protocol.CmdPushFile {
		t.Fatalf("packet.Command = %d, want %d", packet.Command, protocol.CmdPushFile)
	}

	if got, want := packet.Payload[0], byte(len("app.efi")); got != want {
		t.Fatalf("payload filename length = %d, want %d", got, want)
	}
	if got, want := string(packet.Payload[1:1+len("app.efi")]), "app.efi"; got != want {
		t.Fatalf("payload filename = %q, want %q", got, want)
	}
	if got, want := string(packet.Payload[1+len("app.efi"):]), "DATA"; got != want {
		t.Fatalf("payload data = %q, want %q", got, want)
	}
}
