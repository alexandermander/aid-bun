package server

import (
	"context"
	"errors"
	"fmt"
	"net"

	"userve/internal/session"
)

const (
	DefaultHost = "0.0.0.0"
	DefaultPort = 8080
)

type Server struct {
	host     string
	port     int
	sessions *session.Manager
	listener net.Listener
}

func New(host string, port int, sessions *session.Manager) *Server {
	return &Server{
		host:     host,
		port:     port,
		sessions: sessions,
	}
}

func (s *Server) ListenAndServe(ctx context.Context) error {
	listener, err := net.Listen("tcp", fmt.Sprintf("%s:%d", s.host, s.port))
	if err != nil {
		return fmt.Errorf("failed to start TCP server: %w", err)
	}
	s.listener = listener
	defer listener.Close()

	go func() {
		<-ctx.Done()
		_ = listener.Close()
	}()

	for {
		conn, err := listener.Accept()
		if err != nil {
			if ctx.Err() != nil || errors.Is(err, net.ErrClosed) {
				return nil
			}
			fmt.Printf("accept error: %v\n", err)
			continue
		}
		go handleConnection(conn, s.sessions)
	}
}
