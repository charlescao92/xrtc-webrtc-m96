var remoteVideo = document.getElementById("remoteVideo");
var pullBtn = document.getElementById("startPullBtn");
var stopPullBtn = document.getElementById("stopPullBtn");

pullBtn.addEventListener("click", startPull);
stopPullBtn.addEventListener("click", stopPull);

var pullUid;
var pullStreamName;
var pullOffer = "";
var pullPc;
var remoteStream;
var pullLastConnectionState = "";

function InitPull() {
    pullUid = $("#pullUid").val()
    if (pullUid == "") {
        window.alert("请先输入拉流的用户UID!")
        return false;
    }
 
    pullStreamName = $("#pullStreamName").val()
    if (pullStreamName == "") {
        window.alert("请先输入拉流的流ID名称!")
        return false;
    }

    return true;
}

function startPull() {
    if(!InitPull()) {
        return;
    }

    const config = {}
    pullPc = new RTCPeerConnection(config);
    pullPc.createOffer().then(
        pullCreateSessionDescriptionSuccess,
        pullCreateSessionDescriptionError               
    );
}

function stopPull() {
    console.log("pull: send stop pull: /signaling/stoppull");

    remoteVideo.srcObject = null;
    remoteStream = null;

    if (pullPc) {
        pullPc.close();
        pullPc = null;
    }

    $("#pullTips1").html("");
    $("#pullTips2").html("");
    $("#pullTips3").html("");

    $.post("/signaling/stoppull",
        {"uid": pullUid, "streamName": pullStreamName},
        function(data, textStatus) {
            console.log("pull: stop pull response: " + JSON.stringify(data));
            if ("success" == textStatus && 0 == data.errNo) {
                $("#pullTips1").html("<font color='blue'>停止拉流请求成功!</font>");
            } else {
                $("#pullTips1").html("<font color='red'>停止拉流请求失败!</font>");
            }
        },
        "json"
    );

}

function pullStream(answer) {
    pullPc.oniceconnectionstatechange = function(e) {
        var state = "";
        if (pullLastConnectionState != "") {
            state = pullLastConnectionState + "->" + pullPc.iceConnectionState;
        } else {
            state = pullPc.iceConnectionState;
        }

        $("#pullTips2").html("连接状态: " + state);
        pullLastConnectionState = pullPc.iceConnectionState;
    }
    
    pullPc.onaddstream = function(e) {
        remoteStream = e.stream;
        remoteVideo.srcObject = e.stream;
    }
    
    console.log("pull: set remote sdp start");

    pullPc.setRemoteDescription(answer).then(
        pullSetRemoteDescriptionSuccess,
        pullSetRemoteDescriptionError
    );
}

function pullSetRemoteDescriptionSuccess() {
    console.log("pull: pc set remote sdp success");
}

function pullCreateSessionDescriptionSuccess(offer) {
    console.log("pull: offer sdp: \n" + offer.sdp);
    console.log("pull: pc set local sdp start");
    pullPc.setLocalDescription(offer).then(
        pullSetLocalDescriptionSuccess,
        pullSetLocalDescriptionError
    );

    sendPullRequest(offer);
}

function sendPullRequest(offer) {
    console.log("pull: send pull: /signaling/pull");

    $.post("/signaling/pull",
        {"uid": pullUid, "streamName": pullStreamName, "audio": 1, "video": 1, "sdp": offer.sdp},
        function(data, textStatus) {
            console.log("pull: response: " + JSON.stringify(data));
            if ("success" == textStatus && 0 == data.errNo) {
                $("#pullTips1").html("<font color='blue'>拉流请求成功!</font>");
                console.log("pull: answer sdp: \n" + data.data.sdp);
                pullStream(data.data);
            } else {
                $("#pullTips1").html("<font color='red'>拉流请求失败!</font>");
            }
        },
        "json"
    );
}

function pullCreateSessionDescriptionError(error) {
    console.log("pull: pc create local offer error: " + error);
}

function pullSetLocalDescriptionSuccess() {
    console.log("pull: set local sdp success");
}

function pullSetRemoteDescriptionError(error) {
    console.log("pull: pc set remote sdp error: " + error);
}

function pullSetLocalDescriptionError(error) {
    console.log("pull: pc set local sdp error: " + error);
}

