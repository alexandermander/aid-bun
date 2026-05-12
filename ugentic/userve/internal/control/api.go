package control

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"time"

	"userve/internal/commands"
)

const DefaultSocketPath = "/tmp/userve.sock"

type API struct {
	service commands.Service
	server  *http.Server
}

type response struct {
	OK      bool        `json:"ok"`
	Message string      `json:"message,omitempty"`
	Data    interface{} `json:"data,omitempty"`
}

type sendRequest struct {
	Text string `json:"text"`
}

type pushRequest struct {
	Filename string `json:"filename"`
	Data     []byte `json:"data"`
}

func NewAPI(service commands.Service, socketPath string) *API {
	mux := http.NewServeMux()
	api := &API{
		service: service,
		server: &http.Server{
			Handler:           mux,
			ReadHeaderTimeout: 5 * time.Second,
		},
	}

	mux.HandleFunc("/status", api.handleStatus)
	mux.HandleFunc("/outputs", api.handleOutputs)
	mux.HandleFunc("/apps", api.handleApps)
	mux.HandleFunc("/disconnect", api.handleDisconnect)
	mux.HandleFunc("/run", api.handleRun)
	mux.HandleFunc("/send", api.handleSend)
	mux.HandleFunc("/push", api.handlePush)

	api.server.BaseContext = func(net.Listener) context.Context {
		return context.Background()
	}

	return api
}

func (a *API) ListenAndServe(socketPath string) error {
	if err := os.MkdirAll(filepath.Dir(socketPath), 0o755); err != nil {
		return fmt.Errorf("create control socket dir: %w", err)
	}
	if err := os.Remove(socketPath); err != nil && !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("remove stale control socket: %w", err)
	}

	listener, err := net.Listen("unix", socketPath)
	if err != nil {
		return fmt.Errorf("listen on control socket: %w", err)
	}
	defer listener.Close()
	defer os.Remove(socketPath)

	if err := os.Chmod(socketPath, 0o600); err != nil {
		return fmt.Errorf("chmod control socket: %w", err)
	}

	if err := a.server.Serve(listener); err != nil && !errors.Is(err, http.ErrServerClosed) {
		return err
	}
	return nil
}

func (a *API) Shutdown(ctx context.Context) error {
	return a.server.Shutdown(ctx)
}

func (a *API) handleStatus(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeJSON(w, http.StatusMethodNotAllowed, response{Message: "method not allowed"})
		return
	}
	writeJSON(w, http.StatusOK, response{OK: true, Data: a.service.Status()})
}

func (a *API) handleOutputs(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeJSON(w, http.StatusMethodNotAllowed, response{Message: "method not allowed"})
		return
	}

	limit := 0
	if raw := r.URL.Query().Get("limit"); raw != "" {
		value, err := strconv.Atoi(raw)
		if err != nil || value < 0 {
			writeJSON(w, http.StatusBadRequest, response{Message: "invalid limit"})
			return
		}
		limit = value
	}

	outputs, err := a.service.Outputs(limit)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, response{Message: err.Error()})
		return
	}
	writeJSON(w, http.StatusOK, response{OK: true, Data: map[string][]string{"outputs": outputs}})
}

func (a *API) handleApps(w http.ResponseWriter, r *http.Request) {
	a.handleAction(w, r, http.MethodPost, func() (interface{}, error) {
		return a.service.Apps()
	})
}

func (a *API) handleDisconnect(w http.ResponseWriter, r *http.Request) {
	a.handleAction(w, r, http.MethodPost, func() (interface{}, error) {
		return a.service.Disconnect()
	})
}

func (a *API) handleRun(w http.ResponseWriter, r *http.Request) {
	a.handleAction(w, r, http.MethodPost, func() (interface{}, error) {
		return a.service.Run()
	})
}

func (a *API) handleSend(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, response{Message: "method not allowed"})
		return
	}

	var req sendRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, response{Message: "invalid request body"})
		return
	}

	message, err := a.service.Send(req.Text)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, response{Message: err.Error()})
		return
	}
	writeJSON(w, http.StatusOK, response{OK: true, Message: message})
}

func (a *API) handlePush(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, response{Message: "method not allowed"})
		return
	}

	var req pushRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, response{Message: "invalid request body"})
		return
	}

	message, err := a.service.Push(req.Filename, req.Data)
	if err != nil {
		writeJSON(w, http.StatusBadRequest, response{Message: err.Error()})
		return
	}
	writeJSON(w, http.StatusOK, response{OK: true, Message: message})
}

func (a *API) handleAction(w http.ResponseWriter, r *http.Request, method string, fn func() (interface{}, error)) {
	if r.Method != method {
		writeJSON(w, http.StatusMethodNotAllowed, response{Message: "method not allowed"})
		return
	}

	result, err := fn()
	if err != nil {
		writeJSON(w, http.StatusBadRequest, response{Message: err.Error()})
		return
	}

	switch value := result.(type) {
	case string:
		writeJSON(w, http.StatusOK, response{OK: true, Message: value})
	default:
		writeJSON(w, http.StatusOK, response{OK: true, Data: value})
	}
}

func writeJSON(w http.ResponseWriter, status int, body response) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(body)
}
