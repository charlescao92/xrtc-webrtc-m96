package xrpc

import (
	"net"
	"time"
	"bufio"
)

const (
	defaultConnectTimeout = 100 * time.Millisecond
	defaultReadTimeout    = 500 * time.Millisecond
	defaultWriteTimeout   = 500 * time.Millisecond
)

type Client struct {
	ConnectTimeout time.Duration
	ReadTimeout    time.Duration
	WriteTimeout   time.Duration

	selector ServerSelector
}

func NewClient(servers []string) *Client {
	ss := new(RoundRobinSelector)
	ss.SetServers(servers)
	return &Client{
		selector: ss,
	}
}

func (c *Client) connectTimeout() time.Duration {
	if c.ConnectTimeout == 0 {
		return defaultConnectTimeout
	}
	return c.ConnectTimeout
}

func (c *Client) readTimeout() time.Duration {
	if c.ReadTimeout == 0 {
		return defaultReadTimeout
	}
	return c.ReadTimeout
}

func (c *Client) writeTimeout() time.Duration {
	if c.WriteTimeout == 0 {
		return defaultWriteTimeout
	}
	return c.WriteTimeout
}

func (c *Client) Do(req *Request) (*Response, error) {
	addr, err := c.selector.PickServer()
	if err != nil {
		return nil, err
	}

	// 建立连接
	nc, err := net.DialTimeout(addr.Network(), addr.String(), c.connectTimeout())
	if err != nil {
		return nil, err
	}

	// 设置超时
	nc.SetReadDeadline(time.Now().Add(c.readTimeout()))
	nc.SetWriteDeadline(time.Now().Add(c.writeTimeout()))

	// 发起请求
	rw := bufio.NewReadWriter(bufio.NewReader(nc), bufio.NewWriter(nc))
	if _, err := req.Write(rw); err != nil {
		return nil, err
	}

	if err = rw.Flush(); err != nil {
		return nil, err
	}

	// 读取响应结果
	resp, err := ReadResponse(rw);
	if err != nil {
		return nil, err
	}

	nc.Close()

	return resp, nil
}
