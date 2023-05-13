var localVideo = document.getElementById("localVideo");

var pushBtn = document.getElementById("startPushBtn")
pushBtn.addEventListener("click", startPush);

var stopPushBtn = document.getElementById("stopPushBtn");
stopPushBtn.addEventListener("click", stopPush);

//取input控件的值
var pushUid;
var pushStreamName;
var pushAudio = 1;
var pushVideo = 1;

var pushPc;
var localStream;
var pushLastConnectionState = "";

var constraints = {};

var isPushScreen = true;

function InitPush() {
    pushUid = $("#pushUid").val()
    if (pushUid == "") {
        window.alert("请先输入推流的用户UID!")
        return false;
    }
 
    pushStreamName = $("#pushStreamName").val()
    if (pushStreamName == "") {
        window.alert("请先输入推流的流ID名称!")
        return false;
    }

    if (!document.getElementById("audioCheckbox").checked) {
        pushAudio = 0;
    }

    if (document.getElementById("pushCameraRadio").checked) {
        isPushScreen = false;
    }

    if (pushAudio == 1 && pushVideo == 1) {
        constraints = {
            video : true,
            audio : true,
        } 
    } else if (pushAudio == 0 && pushVideo == 1) {
        constraints = {
            video : true,
            audio : false,
        } 
    }

    return true;
}

function startPush() {
    if (!InitPush()) {
        return;
    }

    console.log("push: send push: /signaling/push")

    const config = {}
    pushPc = new RTCPeerConnection(config)

    if (isPushScreen) {
        startPushScreenStream();
    } else {
        startPushCameraStream();
    }
}

function stopPush() {
    console.log("push: send stop push: /signaling/stoppush");

    localVideo.srcObject = null;
    localStream = null;

    if (pushPc) {
        pushPc.close();
        pushPc = null;
    }

    $("#pushTips1").html("");
    $("#pushTips2").html("");
    $("#pushTips3").html("");

    $.post("/signaling/stoppush",
        {"uid": pushUid, "streamName": pushStreamName},
        function(data, textStatus) {
            console.log("stop push response: " + JSON.stringify(data));
            if ("success" == textStatus && 0 == data.errNo) {
                $("#pushTips1").html("<font color='blue'>停止推流请求成功!</font>");
            } else {
                $("#pushTips1").html("<font color='red'>停止推流请求失败!</font>");
            }
        },
        "json"
    );
}

function pushStream(answer) {
    pushPc.oniceconnectionstatechange = function(e) {
        var state = "";
        if (pushLastConnectionState != "") {
            state = pushLastConnectionState + "->" + pushPc.iceConnectionState;
        } else {
            state = pushPc.iceConnectionState;
        }

        $("#pushTips2").html("连接状态: " + state);
        pushLastConnectionState = pushPc.iceConnectionState;
    }
    pushPc.setRemoteDescription(answer).then(
        pushSetRemoteDescriptionSuccess,
        pushSetRemoteDescriptionError
    );
}

function pushSetRemoteDescriptionSuccess() {
    console.log("push: pc set remote description success");
}

function pushSetRemoteDescriptionError(error) {
    console.log("push: pc set remote description error: " + error);
}

function pushSetLocalDescriptionSuccess() {
    console.log("push: pc set local description success");
}

function pushSetLocalDescriptionError(error) {
    console.log("push: pc set local description error: " + error);
}

function pushCreateSessionDescriptionSuccess(offer) {
    console.log("push: offer sdp: \n" + offer.sdp);

    console.log("push: pc set local sdp");

    pushPc.setLocalDescription(offer).then(
        pushSetLocalDescriptionSuccess,
        pushSetLocalDescriptionError
    )

    sendPushRequest(offer);
}

function sendPushRequest(offer) {
    console.log("push: send push: /signaling/push");

    $.post("/signaling/push", 
        {"uid":pushUid, "streamName":pushStreamName, "audio":pushAudio, "video":pushVideo, "sdp":offer.sdp},
        function(data, textStatus) {
            console.log("push response: " + JSON.stringify(data))
            if ("success" == textStatus && 0 == data.errNo) {
                $("#pushTips1").html("<font color='blue'>推流请求成功!</font>")
                console.log("remote answer: \r\n" + data.data.sdp)
                pushStream(data.data);
            } else {
                $("#pushTips1").html("<font color='red'>推流请求失败!</font>")
            }
        },
        "json"
    );
}

function pushCreateSessionDescriptionError(error) {
    console.log("push: pc create answer error: " + error);
}

function gotMediaStream(stream) {
    console.log("push: 用户同意屏幕共享");
    localVideo.srcObject = stream;
    localStream = stream;
    pushPc.addStream(stream);
    pushPc.createOffer().then(
        pushCreateSessionDescriptionSuccess,
        pushCreateSessionDescriptionError       
    );
}

function gotMediaStreamError(error) {
    console.log("push: 用户不同意屏幕共享: " + error);
}

function startPushScreenStream() {
    if(!navigator.mediaDevices ||
	    !navigator.mediaDevices.getDisplayMedia) {
		console.log('navigator.mediaDevices.getDisplayMedia is not supported!');

	} else {
		navigator.mediaDevices.getDisplayMedia(constraints)
			.then(gotMediaStream)
			.catch(gotMediaStreamError);
	}
}

function startPushCameraStream() {
    if(!navigator.mediaDevices ||
	    !navigator.mediaDevices.getUserMedia) {
		console.log('navigator.mediaDevices.getUserMedia is not supported!');

	} else {
		navigator.mediaDevices.getUserMedia(constraints)
			.then(gotMediaStream)
			.catch(gotMediaStreamError);
	}
}
