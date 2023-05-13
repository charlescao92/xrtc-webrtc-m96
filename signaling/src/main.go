package main

import (
	"flag"
	"signaling/src/framework"
)

func startHttp() {
	err := framework.StartHttp()
	if err != nil {
		panic(err)
	}
}

func startHttps() {
	err := framework.StartHttps()
	if err != nil {
		panic(err)
	}
}

func main() {
	// 如果不加会报错ERROR: logging before flag.Parse: I0121 20:52:37.896439   29188 main.go:14] start signal server...
	flag.Parse()

	err := framework.Init("./conf/framework.conf")
	if err != nil {
		panic(err)
	}

	// 静态资源处理 /static
	framework.RegisterStaticUrl()

	//glog.Info("start signal server...")
	// 需要主动刷新，否则会过段时间再写入磁盘
	//glog.Flush()

	go startHttp()

	startHttps()
}
