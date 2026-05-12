package protocol

import (
	"bytes"
	"testing"
)

func TestBuildAndReadPacket(t *testing.T) {
	data := BuildPacket(CmdOutputText, []byte("hello"))

	packet, err := ReadPacket(bytes.NewReader(data))
	if err != nil {
		t.Fatalf("ReadPacket() error = %v", err)
	}

	if packet.Command != CmdOutputText {
		t.Fatalf("packet.Command = %d, want %d", packet.Command, CmdOutputText)
	}
	if string(packet.Payload) != "hello" {
		t.Fatalf("packet.Payload = %q, want %q", packet.Payload, "hello")
	}
}
