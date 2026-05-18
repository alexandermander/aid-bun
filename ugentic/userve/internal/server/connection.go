package server

import (
	"errors"
	"fmt"
	"io"
	"net"

	"userve/internal/protocol"
	"userve/internal/session"
)

func handleConnection(conn net.Conn, sessions *session.Manager) {
	defer conn.Close()

	sessions.Register(conn)
	host, _, err := net.SplitHostPort(conn.RemoteAddr().String())
	if err != nil {
		host = conn.RemoteAddr().String()
	}
	fmt.Printf("[!] System connected from %s\n", host)

	defer func() {
		sessions.Unregister()
		fmt.Println("[!] System disconnected.")
	}()

	for {
		pkt, err := protocol.ReadPacket(conn)
		if err != nil {
			if !errors.Is(err, io.EOF) {
				sessions.SetError("ERR " + err.Error())
			}
			return
		}

		switch pkt.Command {
		case protocol.CmdConnectSession:
			msg := string(pkt.Payload)
			sessions.MarkReady(msg)
			fmt.Printf("[+] Session ready: %s\n", msg)
		case protocol.CmdOutputText:
			text := string(pkt.Payload)
			sessions.AddOutput(text)
			fmt.Printf("[OUTPUT] %s\n", text)
		case protocol.CmdDisconnectSession:
			return
		}
	}
}
