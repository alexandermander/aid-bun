package control

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"net/http"
	"time"

	"userve/internal/session"
)

type Client struct {
	httpClient *http.Client
	baseURL    string
}

type clientResponse struct {
	OK      bool            `json:"ok"`
	Message string          `json:"message"`
	Data    json.RawMessage `json:"data"`
}

func NewClient(socketPath string) *Client {
	transport := &http.Transport{
		DialContext: func(ctx context.Context, _, _ string) (net.Conn, error) {
			var dialer net.Dialer
			return dialer.DialContext(ctx, "unix", socketPath)
		},
	}

	return &Client{
		httpClient: &http.Client{
			Transport: transport,
			Timeout:   10 * time.Second,
		},
		baseURL: "http://unix",
	}
}

func (c *Client) Status(ctx context.Context) (session.Status, error) {
	var status session.Status
	if err := c.do(ctx, http.MethodGet, "/status", nil, &status); err != nil {
		return session.Status{}, err
	}
	return status, nil
}

func (c *Client) Outputs(ctx context.Context, limit int) ([]string, error) {
	path := "/outputs"
	if limit > 0 {
		path = fmt.Sprintf("%s?limit=%d", path, limit)
	}

	var payload struct {
		Outputs []string `json:"outputs"`
	}
	if err := c.do(ctx, http.MethodGet, path, nil, &payload); err != nil {
		return nil, err
	}
	return payload.Outputs, nil
}

func (c *Client) Apps(ctx context.Context) (string, error) {
	return c.messageOnly(ctx, http.MethodPost, "/apps", nil)
}

func (c *Client) Disconnect(ctx context.Context) (string, error) {
	return c.messageOnly(ctx, http.MethodPost, "/disconnect", nil)
}

func (c *Client) Run(ctx context.Context) (string, error) {
	return c.messageOnly(ctx, http.MethodPost, "/run", nil)
}

func (c *Client) Send(ctx context.Context, text string) (string, error) {
	return c.messageOnly(ctx, http.MethodPost, "/send", map[string]string{"text": text})
}

func (c *Client) Push(ctx context.Context, filename string, data []byte) (string, error) {
	return c.messageOnly(ctx, http.MethodPost, "/push", struct {
		Filename string `json:"filename"`
		Data     []byte `json:"data"`
	}{
		Filename: filename,
		Data:     data,
	})
}

func (c *Client) messageOnly(ctx context.Context, method, path string, body interface{}) (string, error) {
	var response clientResponse
	if err := c.doRaw(ctx, method, path, body, &response); err != nil {
		return "", err
	}
	return response.Message, nil
}

func (c *Client) do(ctx context.Context, method, path string, body interface{}, out interface{}) error {
	var response clientResponse
	if err := c.doRaw(ctx, method, path, body, &response); err != nil {
		return err
	}
	if out == nil || len(response.Data) == 0 {
		return nil
	}
	if err := json.Unmarshal(response.Data, out); err != nil {
		return fmt.Errorf("decode response: %w", err)
	}
	return nil
}

func (c *Client) doRaw(ctx context.Context, method, path string, body interface{}, out *clientResponse) error {
	var reader *bytes.Reader
	if body == nil {
		reader = bytes.NewReader(nil)
	} else {
		data, err := json.Marshal(body)
		if err != nil {
			return fmt.Errorf("encode request: %w", err)
		}
		reader = bytes.NewReader(data)
	}

	req, err := http.NewRequestWithContext(ctx, method, c.baseURL+path, reader)
	if err != nil {
		return fmt.Errorf("build request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return fmt.Errorf("request control API: %w", err)
	}
	defer resp.Body.Close()

	if err := json.NewDecoder(resp.Body).Decode(out); err != nil {
		return fmt.Errorf("decode response: %w", err)
	}
	if resp.StatusCode >= 400 || !out.OK {
		if out.Message == "" {
			out.Message = "request failed"
		}
		return errors.New(out.Message)
	}
	return nil
}
