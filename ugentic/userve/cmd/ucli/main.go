package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"userve/internal/control"
)

func main() {
	socketPath := flag.String("control-socket", control.DefaultSocketPath, "Unix socket used to reach userver")
	flag.Parse()

	args := flag.Args()
	if len(args) == 0 {
		usage(os.Stderr)
		os.Exit(1)
	}

	client := control.NewClient(*socketPath)
	ctx := context.Background()

	var err error
	switch args[0] {
	case "status":
		err = runStatus(ctx, client)
	case "outputs":
		err = runOutputs(ctx, client, args[1:])
	case "apps":
		err = printMessage(ctx, client.Apps)
	case "disconnect":
		err = printMessage(ctx, client.Disconnect)
	case "run":
		err = printMessage(ctx, client.Run)
	case "send":
		err = runSend(ctx, client, args[1:])
	case "push":
		err = runPush(ctx, client, args[1:])
	case "help":
		usage(os.Stdout)
		return
	default:
		err = fmt.Errorf("unknown command %q", args[0])
	}

	if err != nil {
		fmt.Fprintf(os.Stderr, "ucli: %v\n", err)
		os.Exit(1)
	}
}

func runStatus(ctx context.Context, client *control.Client) error {
	status, err := client.Status(ctx)
	if err != nil {
		return err
	}
	if !status.Connected {
		fmt.Println("connected=false")
		return nil
	}

	fmt.Printf("connected=true\naddress=%s\nready=%t\n", status.Address, status.Ready)
	if status.ConnectMessage != "" {
		fmt.Printf("connect_message=%s\n", status.ConnectMessage)
	}
	return nil
}

func runOutputs(ctx context.Context, client *control.Client, args []string) error {
	limit := 0
	if len(args) > 1 {
		return fmt.Errorf("usage: ucli outputs [limit]")
	}
	if len(args) == 1 {
		var parseErr error
		_, parseErr = fmt.Sscanf(args[0], "%d", &limit)
		if parseErr != nil || limit < 0 {
			return fmt.Errorf("invalid limit %q", args[0])
		}
	}

	outputs, err := client.Outputs(ctx, limit)
	if err != nil {
		return err
	}
	for _, line := range outputs {
		fmt.Println(line)
	}
	return nil
}

func runSend(ctx context.Context, client *control.Client, args []string) error {
	if len(args) == 0 {
		return fmt.Errorf("usage: ucli send <text>")
	}
	message, err := client.Send(ctx, strings.Join(args, " "))
	if err != nil {
		return err
	}
	fmt.Println(message)
	return nil
}

func runPush(ctx context.Context, client *control.Client, args []string) error {
	if len(args) != 1 {
		return fmt.Errorf("usage: ucli push <file>")
	}

	filename := filepath.Base(args[0])
	data, err := os.ReadFile(args[0])
	if err != nil {
		return err
	}

	message, err := client.Push(ctx, filename, data)
	if err != nil {
		return err
	}
	fmt.Println(message)
	return nil
}

func printMessage(ctx context.Context, fn func(context.Context) (string, error)) error {
	message, err := fn(ctx)
	if err != nil {
		return err
	}
	fmt.Println(message)
	return nil
}

func usage(w *os.File) {
	fmt.Fprintln(w, "usage: ucli [--control-socket /tmp/userve.sock] <command> [args]")
	fmt.Fprintln(w, "commands: status, outputs [limit], apps, push <file>, run, send <text>, disconnect, help")
}
