package protocol

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io"


type Packet struct {
	Command byte
	Payload []byte
}

func ReadPacket(r io.Reader) (*Packet, error) {
	header := make([]byte, HeaderSize)
	if _, err := io.ReadFull(r, header); err != nil {
		if errors.Is(err, io.EOF) || errors.Is(err, io.ErrUnexpectedEOF) {
			return nil, io.EOF
		}
		return nil, err
	}

	payloadLength := int(binary.LittleEndian.Uint16(header[1:]))
	payload := make([]byte, payloadLength)
	if _, err := io.ReadFull(r, payload); err != nil {
		if errors.Is(err, io.EOF) || errors.Is(err, io.ErrUnexpectedEOF) {
			return nil, fmt.Errorf("connection closed during payload read, expected %d bytes", payloadLength)
		}
		return nil, err
	}

	return &Packet{
		Command: header[0],
		Payload: payload,
	}, nil
}

func BuildPacket(command byte, payload []byte) []byte {
	data := make([]byte, HeaderSize+len(payload))
	data[0] = command
	binary.LittleEndian.PutUint16(data[1:3], uint16(len(payload)))
	copy(data[3:], payload)
	return data
}

func IsASCII(text string) bool {
	for _, r := range text {
		if r > 127 {
			return false
		}
	}
	return true
}

func BuildPushPayload(filename string, data []byte) ([]byte, error) {
	if filename == "" {
		return nil, errors.New("missing filename")
	}
	if !IsASCII(filename) {
		return nil, errors.New("filename must be ASCII")
	}
	if len(filename) > 255 {
		return nil, errors.New("filename too long")
	}

	payload := make([]byte, 1+len(filename)+len(data))
	payload[0] = byte(len(filename))
	copy(payload[1:], []byte(filename))
	copy(payload[1+len(filename):], data)
	return payload, nil
}
