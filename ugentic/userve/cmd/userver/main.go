package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"

	"userve/internal/commands"
	"userve/internal/control"
	"userve/internal/server"
	"userve/internal/session"
)

func main() {
	tcpPort := flag.Int("tcp-port", server.DefaultPort, "TCP port to listen on for EFI clients")
	controlSocket := flag.String("control-socket", control.DefaultSocketPath, "Unix socket used by local operator CLI")
	flag.Parse()

	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()

	sessions := &session.Manager{}
	service := commands.Service{Sessions: sessions}
	tcpServer := server.New(server.DefaultHost, *tcpPort, sessions)
	controlAPI := control.NewAPI(service, *controlSocket)

	serverErrs := make(chan error, 2)

	go func() {
		serverErrs <- tcpServer.ListenAndServe(ctx)
	}()
	go func() {
		serverErrs <- controlAPI.ListenAndServe(*controlSocket)
	}()

	fmt.Printf("Listening for EFI connections on port %d...\n", *tcpPort)
	fmt.Printf("Listening for local control on %s...\n", *controlSocket)

	select {
	case <-ctx.Done():
	case err := <-serverErrs:
		if err != nil {
			fmt.Fprintf(os.Stderr, "userver failed: %v\n", err)
			os.Exit(1)
		}
	}

	cancel()

	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer shutdownCancel()

	if err := controlAPI.Shutdown(shutdownCtx); err != nil {
		fmt.Fprintf(os.Stderr, "control shutdown failed: %v\n", err)
		os.Exit(1)
	}
}
