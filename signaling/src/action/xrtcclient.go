package action

import (
	"fmt"
	"net/http"
	"text/template"

	"signaling/src/framework"
)

type xrtcClientAction struct{}

func NewXrtcClientAction() *xrtcClientAction {
	return &xrtcClientAction{}
}

func (*xrtcClientAction) Execute(w http.ResponseWriter, cr *framework.ComRequest) {
	r := cr.R

	// 解析html模板
	t, err := template.ParseFiles(framework.GetStaticDir() + "/template/index.html")
	if err != nil {
		fmt.Println(err)
		writeHtmlErrorResponse(w, http.StatusNotFound, "404-Not Found")
		return
	}

	// 获取请求的动态参数
	request := make(map[string]string)
	for k, v := range r.Form {
		request[k] = v[0]
	}

	// 将动态参数设置到模板里面
	// http://192.168.1.19:8080/xrtcweb
	// https://192.168.1.19:8081/xrtcweb
	err = t.Execute(w, request)
	if err != nil {
		fmt.Println(err)
		writeHtmlErrorResponse(w, http.StatusNotFound, "404-Not Found")
		return
	}
}
