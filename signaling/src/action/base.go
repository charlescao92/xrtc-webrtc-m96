package action

import (
	"bytes"
	"encoding/json"
	"net/http"
	"signaling/src/comerrors"
	"signaling/src/framework"
	"strconv"
)

const (
	CMDNO_PUSH      = 1
	CMDNO_PULL      = 2
	CMDNO_ANSWER    = 3
	CMDNO_STOP_PUSH = 4
	CMDNO_STOP_PULL = 5
)

type comHttpResp struct {
	ErrNo  int         `json:"errNo"`
	ErrMsg string      `json:"errMsg"`
	Data   interface{} `json:"data"`
}

func writeJsonErrorResponse(cerr *comerrors.Errors, w http.ResponseWriter, cr *framework.ComRequest) {
	cr.Logger.AddNotice("errNo", strconv.Itoa(cerr.Errno()))
	cr.Logger.AddNotice("errMsg", cerr.Error())
	cr.Logger.Warningf("request process failed")

	resp := comHttpResp{
		ErrNo:  cerr.Errno(),
		ErrMsg: "process error",
	}

	buffer := new(bytes.Buffer)
	json.NewEncoder(buffer).Encode(resp)
	w.Write(buffer.Bytes())

}

func writeHtmlErrorResponse(w http.ResponseWriter, status int, err string) {
	w.WriteHeader(status)
	w.Write([]byte(err))
}
