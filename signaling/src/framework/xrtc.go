package framework

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"signaling/src/framework/xrpc"
	"strconv"
	"strings"
	"time"
)

var xrpcClients map[string]*xrpc.Client = make(map[string]*xrpc.Client)

func loadXrpc() error {
	sections := configFile.GetSectionList()
	//fmt.Println(sections)

	for _, section := range sections {
		if !strings.HasPrefix(section, "xrpc.") {
			continue
		}

		mSection, err := configFile.GetSection(section)
		if err != nil {
			return err
		}

		// server
		values, ok := mSection["server"]
		if !ok {
			return errors.New("no server field in config file")
		}

		arrServer := strings.Split(values, ",")
		//fmt.Println(arrServer)
		for i, server := range arrServer {
			arrServer[i] = strings.TrimSpace(server)
		}

		client := xrpc.NewClient(arrServer)

		if value, ok := mSection["connectTimeout"]; ok {
			if connectTimeout, err := strconv.Atoi(value); err == nil {
				client.ConnectTimeout = time.Duration(connectTimeout) * time.Millisecond
			}
		}

		if value, ok := mSection["readTimeout"]; ok {
			if readTimeout, err := strconv.Atoi(value); err == nil {
				client.ReadTimeout = time.Duration(readTimeout) * time.Millisecond
			}
		}

		if value, ok := mSection["writeTimeout"]; ok {
			if writeTimeout, err := strconv.Atoi(value); err == nil {
				client.WriteTimeout = time.Duration(writeTimeout) * time.Millisecond
			}
		}

		//fmt.Println(client)
		xrpcClients[section] = client
	}

	return nil
}

// 通过查找xrpcClients里面的serviceName对应的Client，来跟xrtcserver进行交互
func Call(serviceName string, resquest interface{}, response interface{}, logId uint32) error {
	fmt.Println("call " + serviceName)

	client, ok := xrpcClients["xrpc."+serviceName]
	if !ok {
		return fmt.Errorf("[%s] service not found", serviceName)
	}

	content, err := json.Marshal(resquest)
	if err != nil {
		return err
	}

	fmt.Println("call rtc requset:", string(content))

	req := xrpc.NewRequest(bytes.NewReader(content), logId)
	resp, err := client.Do(req)
	if err != nil {
		return err
	}

	err = json.Unmarshal(resp.Body, response)
	if err != nil {
		return err
	}

	return nil
}
