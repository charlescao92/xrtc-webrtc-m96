package action

import (
	"encoding/json"
	"fmt"
	"net/http"
	"signaling/src/comerrors"
	"signaling/src/framework"
	"strconv"
)

type pullAction struct {
}

func NewPullAction() *pullAction {
	return &pullAction{}
}

type xrtcPullReq struct {
	Cmdno      int    `json:"cmdno"`
	Uid        uint64 `json:"uid"`
	StreamName string `json:"stream_name"`
	Audio      int    `json:"audio"`
	Video      int    `json:"video"`
	Sdp        string `json:"sdp"`
}

type xrtcPullResp struct {
	ErrNo  int    `json:"err_no"`
	ErrMsg string `json:"err_msg"`
	Offer  string `json:"offer"`
}

type pullData struct {
	Type string `json:"type"`
	Sdp  string `json:"sdp"`
}

func (*pullAction) Execute(w http.ResponseWriter, cr *framework.ComRequest) {
	r := cr.R

	// uid
	var strUid string
	if values, ok := r.Form["uid"]; ok {
		strUid = values[0]
	}

	uid, err := strconv.ParseUint(strUid, 10, 64)
	if err != nil || uid <= 0 {
		cerr := comerrors.New(comerrors.ParamErr, "parse uid error:"+err.Error())
		writeJsonErrorResponse(cerr, w, cr)
		return
	}

	//streamName
	var streamName string
	if values, ok := r.Form["streamName"]; ok {
		streamName = values[0]
	}

	if "" == streamName {
		cerr := comerrors.New(comerrors.ParamErr, "streamName is null")
		writeJsonErrorResponse(cerr, w, cr)
		return
	}

	// audio , video
	var strAudio, strVideo string
	var audio, video int
	if values, ok := r.Form["audio"]; ok {
		strAudio = values[0]
	}

	if "" == strAudio {
		audio = 0
	} else {
		audio = 1
	}

	if values, ok := r.Form["video"]; ok {
		strVideo = values[0]
	}

	if "" == strVideo {
		video = 0
	} else {
		video = 1
	}

	//sdp
	var sdp string
	if values, ok := r.Form["sdp"]; ok {
		sdp = values[0]
	}

	if "" == sdp {
		cerr := comerrors.New(comerrors.ParamErr, "sdp is null")
		writeJsonErrorResponse(cerr, w, cr)
		return
	}

	req := xrtcPullReq{
		Cmdno:      CMDNO_PULL,
		Uid:        uid,
		StreamName: streamName,
		Audio:      audio,
		Video:      video,
		Sdp:		sdp,
	}

	var resp xrtcPullResp
	err = framework.Call("xrtc", req, &resp, cr.LogId)
	fmt.Printf("%+v\n", resp)
	if err != nil {
		cerr := comerrors.New(comerrors.ParamErr, "backend process error:"+err.Error())
		writeJsonErrorResponse(cerr, w, cr)
		return
	}

	if resp.ErrNo != 0 {
		cerr := comerrors.New(comerrors.NetworkErr,
			fmt.Sprintf("backend process errno: %d", resp.ErrNo))
		writeJsonErrorResponse(cerr, w, cr)
		return
	}

	httpResp := comHttpResp{
		ErrNo:  0,
		ErrMsg: "success",
		Data: pullData{
			Type: "offer",
			Sdp:  resp.Offer,
		},
	}

	b, _ := json.Marshal(httpResp)
	cr.Logger.AddNotice("resp", string(b))
	w.Write(b)

}
