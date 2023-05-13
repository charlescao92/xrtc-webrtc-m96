#ifndef MODULES_RTP_RTCP_RTP_RTCP_DEFINES_H_
#define MODULES_RTP_RTCP_RTP_RTCP_DEFINES_H_

#include <stdint.h>
#include <stdlib.h>

#include <absl/types/optional.h>
#include <api/units/time_delta.h>

namespace xrtc {

class RtpPacket;

constexpr int kDefaultMaxReorderingThreshold = 5;  // In sequence numbers.
constexpr int kRtcpMaxNackFields = 253;

constexpr webrtc::TimeDelta RTCP_SEND_BEFORE_KEY_FRAME = webrtc::TimeDelta::Millis(100);
constexpr int RTCP_MAX_REPORT_BLOCKS = 31;  // RFC 3550 page 37

#define IP_PACKET_SIZE 1500

enum RTCPPayloadType : uint8_t {
    kSR    = 200,
    kRR    = 201,
    kSDES  = 202,
    kBye   = 203,
    kApp   = 204,
    kRtpFb = 205,
    kPsFb  = 206,
    kXR    = 207,
};

enum RTCPPacketType : uint32_t {
    kRtcpReport = 0x0001,
    kRtcpSr = 0x0002,
    kRtcpRr = 0x0004,
    kRtcpNack = 0x0040,
};

class RtpPacketCounter {
public:
    RtpPacketCounter() = default;
    explicit RtpPacketCounter(const RtpPacket& packet);

    void Add(const RtpPacketCounter& other);
    void Subtract(const RtpPacketCounter& other);
    void AddPacket(const RtpPacket& packet);

    size_t header_bytes = 0;
    size_t payload_bytes = 0;
    size_t padding_bytes = 0;
    uint32_t packets = 0;
};

// Data usage statistics for a (rtp) stream.
struct StreamDataCounters {
  StreamDataCounters();

  void Add(const StreamDataCounters& other) {
    transmitted.Add(other.transmitted);
    retransmitted.Add(other.retransmitted);
    fec.Add(other.fec);
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms < first_packet_time_ms ||
         first_packet_time_ms == -1)) {
      // Use oldest time.
      first_packet_time_ms = other.first_packet_time_ms;
    }
  }

  void Subtract(const StreamDataCounters& other) {
    transmitted.Subtract(other.transmitted);
    retransmitted.Subtract(other.retransmitted);
    fec.Subtract(other.fec);
    if (other.first_packet_time_ms != -1 &&
        (other.first_packet_time_ms > first_packet_time_ms ||
         first_packet_time_ms == -1)) {
      // Use youngest time.
      first_packet_time_ms = other.first_packet_time_ms;
    }
  }

  int64_t TimeSinceFirstPacketInMs(int64_t now_ms) const {
    return (first_packet_time_ms == -1) ? -1 : (now_ms - first_packet_time_ms);
  }

  // Returns the number of bytes corresponding to the actual media payload (i.e.
  // RTP headers, padding, retransmissions and fec packets are excluded).
  // Note this function does not have meaning for an RTX stream.
  size_t MediaPayloadBytes() const {
    return transmitted.payload_bytes - retransmitted.payload_bytes -
           fec.payload_bytes;
  }

  int64_t first_packet_time_ms;  // Time when first packet is sent/received.
  // The timestamp at which the last packet was received, i.e. the time of the
  // local clock when it was received - not the RTP timestamp of that packet.
  // https://w3c.github.io/webrtc-stats/#dom-rtcinboundrtpstreamstats-lastpacketreceivedtimestamp
  absl::optional<int64_t> last_packet_received_timestamp_ms;
  RtpPacketCounter transmitted;    // Number of transmitted packets/bytes.
  RtpPacketCounter retransmitted;  // Number of retransmitted packets/bytes.
  RtpPacketCounter fec;            // Number of redundancy packets/bytes.
};

enum class RtpPacketMediaType : size_t {
    kAudio,
    kVideo,
    kRetransmission,
    kForwardErrorCorrection,
    kPadding,
};

// Information exposed through the GetStats api.
struct RtpReceiveStats {
  // `packets_lost` and `jitter` are defined by RFC 3550, and exposed in the
  // RTCReceivedRtpStreamStats dictionary, see
  // https://w3c.github.io/webrtc-stats/#receivedrtpstats-dict*
  int32_t packets_lost = 0;
  uint32_t jitter = 0;

  // Timestamp and counters exposed in RTCInboundRtpStreamStats, see
  // https://w3c.github.io/webrtc-stats/#inboundrtpstats-dict*
  absl::optional<int64_t> last_packet_received_timestamp_ms;
  RtpPacketCounter packet_counter;
};


// This enum must not have any gaps, i.e., all integers between
// kRtpExtensionNone and kRtpExtensionNumberOfExtensions must be valid enum
// entries.
enum RTPExtensionType : int {
  kRtpExtensionNone,
  kRtpExtensionTransmissionTimeOffset,
  kRtpExtensionAudioLevel,
  kRtpExtensionCsrcAudioLevel,
  kRtpExtensionInbandComfortNoise,
  kRtpExtensionAbsoluteSendTime,
  kRtpExtensionAbsoluteCaptureTime,
  kRtpExtensionVideoRotation,
  kRtpExtensionTransportSequenceNumber,
  kRtpExtensionTransportSequenceNumber02,
  kRtpExtensionPlayoutDelay,
  kRtpExtensionVideoContentType,
  kRtpExtensionVideoLayersAllocation,
  kRtpExtensionVideoTiming,
  kRtpExtensionRtpStreamId,
  kRtpExtensionRepairedRtpStreamId,
  kRtpExtensionMid,
  kRtpExtensionGenericFrameDescriptor00,
  kRtpExtensionGenericFrameDescriptor = kRtpExtensionGenericFrameDescriptor00,
  kRtpExtensionGenericFrameDescriptor02,
  kRtpExtensionColorSpace,
  kRtpExtensionVideoFrameTrackingId,
  kRtpExtensionNumberOfExtensions  // Must be the last entity in the enum.
};

} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTP_RTCP_DEFINES_H_
