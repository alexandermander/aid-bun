package commands

import (
	"fmt"

	"userve/internal/protocol"
	"userve/internal/session"
)

type Service struct {
	Sessions *session.Manager
}

func (s Service) Status() session.Status {
	return s.Sessions.Status()
}

func (s Service) Outputs(limit int) ([]string, error) {
	return s.Sessions.Outputs(limit)
}

func (s Service) Apps() (string, error) {
	if err := s.Sessions.SendPacket(protocol.CmdGetApps, nil); err != nil {
		return "", err
	}
	return fmt.Sprintf("sent command %d", protocol.CmdGetApps), nil
}

func (s Service) Disconnect() (string, error) {
	if err := s.Sessions.SendPacket(protocol.CmdDisconnectSession, nil); err != nil {
		return "", err
	}
	return "disconnect requested", nil
}

func (s Service) Run() (string, error) {
	if err := s.Sessions.SendPacket(protocol.CmdExecApp, nil); err != nil {
		return "", err
	}
	return "remote execution requested", nil
}

func (s Service) Send(text string) (string, error) {
	if text == "" {
		return "", fmt.Errorf("empty command")
	}
	if !protocol.IsASCII(text) {
		return "", fmt.Errorf("commands must be ASCII")
	}
	if err := s.Sessions.SendPacket(protocol.CmdEchoSend, []byte(text)); err != nil {
		return "", err
	}
	return fmt.Sprintf("sent command %d", protocol.CmdEchoSend), nil
}

func (s Service) Push(filename string, data []byte) (string, error) {
	payload, err := protocol.BuildPushPayload(filename, data)
	if err != nil {
		return "", err
	}
	if err := s.Sessions.SendPacket(protocol.CmdPushFile, payload); err != nil {
		return "", err
	}
	return fmt.Sprintf("pushed %s", filename), nil
}
