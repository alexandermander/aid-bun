package session

import (
	"fmt"
	"net"
	"sync"

	"userve/internal/protocol"
)

const maxOutputLog = 200

type Status struct {
	Connected      bool   `json:"connected"`
	Address        string `json:"address,omitempty"`
	Ready          bool   `json:"ready,omitempty"`
	ConnectMessage string `json:"connect_message,omitempty"`
}

type state struct {
	conn           net.Conn
	addr           string
	ready          bool
	connectMessage string
	outputs        []string
	lastError      string
}

type Manager struct {
	mu      sync.Mutex
	session *state
}

func (m *Manager) Register(conn net.Conn) {
	m.mu.Lock()
	defer m.mu.Unlock()

	m.session = &state{
		conn:    conn,
		addr:    conn.RemoteAddr().String(),
		outputs: make([]string, 0, maxOutputLog),
	}
}

func (m *Manager) Unregister() {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.session = nil
}

func (m *Manager) IsConnected() bool {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.session != nil
}

func (m *Manager) MarkReady(message string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.session == nil {
		return
	}
	m.session.ready = true
	m.session.connectMessage = message
}

func (m *Manager) AddOutput(text string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.session == nil {
		return
	}
	m.session.outputs = appendRolling(m.session.outputs, text)
}

func (m *Manager) SetError(text string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.session == nil {
		return
	}
	m.session.lastError = text
	m.session.outputs = appendRolling(m.session.outputs, text)
}

func (m *Manager) Status() Status {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.session == nil {
		return Status{}
	}

	return Status{
		Connected:      true,
		Address:        m.session.addr,
		Ready:          m.session.ready,
		ConnectMessage: m.session.connectMessage,
	}
}

func (m *Manager) Outputs(limit int) ([]string, error) {
	m.mu.Lock()
	defer m.mu.Unlock()

	if m.session == nil {
		return nil, fmt.Errorf("no system connected")
	}

	outputs := m.session.outputs
	if limit > 0 && limit < len(outputs) {
		outputs = outputs[len(outputs)-limit:]
	}

	result := make([]string, len(outputs))
	copy(result, outputs)
	return result, nil
}

func (m *Manager) SendPacket(command byte, payload []byte) error {
	m.mu.Lock()
	if m.session == nil {
		m.mu.Unlock()
		return fmt.Errorf("no system connected")
	}
	conn := m.session.conn
	m.mu.Unlock()

	if _, err := conn.Write(protocol.BuildPacket(command, payload)); err != nil {
		m.Unregister()
		return fmt.Errorf("connection lost: %w", err)
	}

	return nil
}

func appendRolling(outputs []string, text string) []string {
	if len(outputs) == maxOutputLog {
		copy(outputs, outputs[1:])
		outputs = outputs[:maxOutputLog-1]
	}
	return append(outputs, text)
}
