package xrpc

import (
	"fmt"
	"io"
)

type Response struct {
	Header Header
	Body   []byte
}

func ReadResponse(r io.Reader) (resp *Response, err error) {
	resp = new(Response)
	if _, err := resp.Header.Read(r); err != nil {
		return nil, err
	}

	if resp.Header.MagicNum != HEADER_MAGICNUM {
		return nil, fmt.Errorf("invalid magic num: %x", resp.Header.MagicNum)
	}

	resp.Body = make([]byte, resp.Header.BodyLen)
	if _, err := io.ReadFull(r, resp.Body); err != nil {
		return nil, err
	}

	return resp, nil
}
