#ifndef MODULES_RTP_RTCP_RTP_PACKET_H_
#define MODULES_RTP_RTCP_RTP_PACKET_H_

#include <stdlib.h>

#include <rtc_base/copy_on_write_buffer.h>
#include <absl/types/optional.h>
#include <api/stats_types.h>

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace xrtc {

class RtpPacket {
public:
    using ExtensionType = RTPExtensionType;
    using ExtensionManager = RtpHeaderExtensionMap;

public:
    RtpPacket();
    RtpPacket(size_t capacity);

    uint32_t ssrc() const { return ssrc_;  }
    uint16_t sequence_number() const { return sequence_number_; }
    bool marker() const { return marker_; }
    uint32_t timestamp() const { return timestamp_; }
    rtc::ArrayView<const uint8_t> payload() const {
        return rtc::MakeArrayView(data() + payload_offset_, payload_size_);
    }

    size_t header_size() const { return payload_offset_; }
    size_t payload_size() const { return payload_size_; }
    size_t padding_size() const { return padding_size_; }

    const uint8_t* data() const { return buffer_.cdata(); }
    size_t size() const {
        return payload_offset_ + payload_size_ + padding_size_;
    }

    size_t capacity() { return buffer_.capacity(); }
    size_t FreeCapacity() { return capacity() - size(); }
    void Clear();

    void SetMarker(bool marker_bit);
    void SetPayloadType(uint8_t payload_type);
    void SetSequenceNumber(uint16_t seq_no);
    void SetTimestamp(uint32_t ts);
    void SetSsrc(uint32_t ssrc);
    uint8_t* SetPayloadSize(size_t bytes_size);

    uint8_t* AllocatePayload(size_t payload_size);

    uint8_t* WriteAt(size_t offset) {
        return buffer_.MutableData() + offset;
    }

    void WriteAt(size_t offset, uint8_t byte) {
        buffer_.MutableData()[offset] = byte;
    }

      // Parse and copy given buffer into Packet.
    // Does not require extension map to be registered (map is only required to
    // read or allocate extensions in methods GetExtension, AllocateExtension,
    // etc.)
    bool Parse(const uint8_t* buffer, size_t size);
    bool Parse(rtc::ArrayView<const uint8_t> packet);

    // Parse and move given buffer into Packet.
    bool Parse(rtc::CopyOnWriteBuffer packet);

    // Header extensions.
    template <typename Extension>
    bool HasExtension() const;
    bool HasExtension(ExtensionType type) const;

    template <typename Extension, typename FirstValue, typename... Values>
    bool GetExtension(FirstValue, Values...) const;

    template <typename Extension>
    absl::optional<typename Extension::value_type> GetExtension() const;

private:
    struct ExtensionInfo {
        explicit ExtensionInfo(uint8_t id) : ExtensionInfo(id, 0, 0) {}
        ExtensionInfo(uint8_t id, uint8_t length, uint16_t offset)
            : id(id), length(length), offset(offset) {}
        uint8_t id;
        uint8_t length;
        uint16_t offset;
    };

    // Helper function for Parse. Fill header fields using data in given buffer,
    // but does not touch packet own buffer, leaving packet in invalid state.
    bool ParseBuffer(const uint8_t* buffer, size_t size);

    // Find an extension `type`.
    // Returns view of the raw extension or empty view on failure.
    rtc::ArrayView<const uint8_t> FindExtension(ExtensionType type) const;

    // Returns pointer to extension info for a given id. Returns nullptr if not
    // found.
    const ExtensionInfo* FindExtensionInfo(int id) const;

    // Returns reference to extension info for a given id. Creates a new entry
    // with the specified id if not found.
    ExtensionInfo& FindOrCreateExtensionInfo(int id);

private:
    bool marker_;
    uint8_t payload_type_;
    uint16_t sequence_number_;
    uint32_t timestamp_;
    uint32_t ssrc_;
    size_t payload_offset_;
    size_t payload_size_;
    size_t padding_size_;
    rtc::CopyOnWriteBuffer buffer_;

    ExtensionManager extensions_;
    size_t extensions_size_ = 0;  // Unaligned.
    std::vector<ExtensionInfo> extension_entries_;
};

template <typename Extension>
bool RtpPacket::HasExtension() const {
  return HasExtension(Extension::kId);
}

template <typename Extension, typename FirstValue, typename... Values>
bool RtpPacket::GetExtension(FirstValue first, Values... values) const {
  auto raw = FindExtension(Extension::kId);
  if (raw.empty())
    return false;
  return Extension::Parse(raw, first, values...);
}

template <typename Extension>
absl::optional<typename Extension::value_type> RtpPacket::GetExtension() const {
  absl::optional<typename Extension::value_type> result;
  auto raw = FindExtension(Extension::kId);
  if (raw.empty() || !Extension::Parse(raw, &result.emplace()))
    result = absl::nullopt;
  return result;
}

} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTP_PACKET_H_