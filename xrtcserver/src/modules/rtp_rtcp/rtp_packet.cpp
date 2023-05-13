#include "modules/rtp_rtcp/rtp_packet.h"

#include <rtc_base/logging.h>
#include <rtc_base/numerics/safe_conversions.h>

#include "modules/rtp_rtcp/source/byte_io.h"

namespace xrtc {

const size_t kDefaultRtpCapacity = 1500;
const size_t kFixedHeaderSize = 12;
const uint8_t kRtpVersion = 2;

constexpr uint16_t kOneByteExtensionProfileId = 0xBEDE;
constexpr uint16_t kTwoByteExtensionProfileId = 0x1000;
constexpr uint16_t kTwobyteExtensionProfileIdAppBitsFilter = 0xfff0;
constexpr size_t kOneByteExtensionHeaderLength = 1;
constexpr size_t kTwoByteExtensionHeaderLength = 2;
constexpr size_t kDefaultPacketSize = 1500;

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            Contributing source (CSRC) identifiers             |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |  header eXtension profile id  |       length in 32bits        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          Extensions                           |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |                           Payload                             |
// |             ....              :  padding...                   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               padding         | Padding size  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	RtpPacket::RtpPacket() : RtpPacket(kDefaultRtpCapacity) {
	}

	RtpPacket::RtpPacket(size_t capacity) :
		buffer_(capacity)
	{
		  Clear();
	}

	void RtpPacket::Clear(){
		marker_ = false;
		payload_type_ = 0;
		sequence_number_ = 0;
		timestamp_ = 0;
		ssrc_ = 0;
		payload_offset_ = kFixedHeaderSize;
		payload_size_ = 0;
		padding_size_ = 0;

		buffer_.SetSize(kFixedHeaderSize);
		// 写入RTP版本信息
		WriteAt(0, kRtpVersion << 6);
	}

	void RtpPacket::SetMarker(bool marker_bit) {
		marker_ = marker_bit;

		if (marker_bit) {
			WriteAt(1, data()[1] | 0x80);
		}
		else {
			WriteAt(1, data()[1] & 0x7F);
		}
	}

	void RtpPacket::SetPayloadType(uint8_t payload_type) {
		payload_type_ = payload_type;

		WriteAt(1, (data()[1] & 0x80 ) | payload_type);
	}

	void RtpPacket::SetSequenceNumber(uint16_t seq_no) {
		sequence_number_ = seq_no;
		webrtc::ByteWriter<uint16_t>::WriteBigEndian(WriteAt(2), seq_no);
	}

	void RtpPacket::SetTimestamp(uint32_t ts) {
		timestamp_ = ts;
		webrtc::ByteWriter<uint32_t>::WriteBigEndian(WriteAt(4), ts);
	}

	void RtpPacket::SetSsrc(uint32_t ssrc) {
		ssrc_ = ssrc;
		webrtc::ByteWriter<uint32_t>::WriteBigEndian(WriteAt(8), ssrc);
	}

	uint8_t* RtpPacket::SetPayloadSize(size_t bytes_size) {
		if (payload_offset_ + bytes_size > capacity()) {
			RTC_LOG(LS_WARNING) << "set payload size failed, no enough space in buffer";
			return nullptr;
		}

		payload_size_ = bytes_size;
		buffer_.SetSize(payload_offset_ + payload_size_);
		return WriteAt(payload_offset_);
	}

uint8_t* RtpPacket::AllocatePayload(size_t payload_size) {
		SetPayloadSize(0);
		return SetPayloadSize(payload_size);
}

bool RtpPacket::Parse(const uint8_t* buffer, size_t buffer_size) {
  if (!ParseBuffer(buffer, buffer_size)) {
    Clear();
    return false;
  }
  buffer_.SetData(buffer, buffer_size);
  RTC_DCHECK_EQ(size(), buffer_size);
  return true;
}

bool RtpPacket::Parse(rtc::ArrayView<const uint8_t> packet) {
  return Parse(packet.data(), packet.size());
}

bool RtpPacket::Parse(rtc::CopyOnWriteBuffer buffer) {
  if (!ParseBuffer(buffer.cdata(), buffer.size())) {
    Clear();
    return false;
  }
  size_t buffer_size = buffer.size();
  buffer_ = std::move(buffer);
  RTC_DCHECK_EQ(size(), buffer_size);
  return true;
}

bool RtpPacket::ParseBuffer(const uint8_t* buffer, size_t size) {
  if (size < kFixedHeaderSize) {
    return false;
  }
  const uint8_t version = buffer[0] >> 6;
  if (version != kRtpVersion) {
    return false;
  }
  const bool has_padding = (buffer[0] & 0x20) != 0;
  const bool has_extension = (buffer[0] & 0x10) != 0;
  const uint8_t number_of_crcs = buffer[0] & 0x0f;
  marker_ = (buffer[1] & 0x80) != 0;
  payload_type_ = buffer[1] & 0x7f;

  sequence_number_ = webrtc::ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
  timestamp_ = webrtc::ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
  ssrc_ = webrtc::ByteReader<uint32_t>::ReadBigEndian(&buffer[8]);
  if (size < kFixedHeaderSize + number_of_crcs * 4) {
    return false;
  }
  payload_offset_ = kFixedHeaderSize + number_of_crcs * 4;

  extensions_size_ = 0;
  extension_entries_.clear();
  if (has_extension) {
    /* RTP header extension, RFC 3550.
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      defined by profile       |           length              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        header extension                       |
    |                             ....                              |
    */
    size_t extension_offset = payload_offset_ + 4;
    if (extension_offset > size) {
      return false;
    }
    uint16_t profile =
        webrtc::ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_]);
    size_t extensions_capacity =
        webrtc::ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_ + 2]);
    extensions_capacity *= 4;
    if (extension_offset + extensions_capacity > size) {
      return false;
    }
    if (profile != kOneByteExtensionProfileId &&
        (profile & kTwobyteExtensionProfileIdAppBitsFilter) !=
            kTwoByteExtensionProfileId) {
      RTC_LOG(LS_WARNING) << "Unsupported rtp extension " << profile;
    } else {
      size_t extension_header_length = profile == kOneByteExtensionProfileId
                                           ? kOneByteExtensionHeaderLength
                                           : kTwoByteExtensionHeaderLength;
      constexpr uint8_t kPaddingByte = 0;
      constexpr uint8_t kPaddingId = 0;
      constexpr uint8_t kOneByteHeaderExtensionReservedId = 15;
      while (extensions_size_ + extension_header_length < extensions_capacity) {
        if (buffer[extension_offset + extensions_size_] == kPaddingByte) {
          extensions_size_++;
          continue;
        }
        int id;
        uint8_t length;
        if (profile == kOneByteExtensionProfileId) {
          id = buffer[extension_offset + extensions_size_] >> 4;
          length = 1 + (buffer[extension_offset + extensions_size_] & 0xf);
          if (id == kOneByteHeaderExtensionReservedId ||
              (id == kPaddingId && length != 1)) {
            break;
          }
        } else {
          id = buffer[extension_offset + extensions_size_];
          length = buffer[extension_offset + extensions_size_ + 1];
        }

        if (extensions_size_ + extension_header_length + length >
            extensions_capacity) {
          RTC_LOG(LS_WARNING) << "Oversized rtp header extension.";
          break;
        }

        ExtensionInfo& extension_info = FindOrCreateExtensionInfo(id);
        if (extension_info.length != 0) {
          RTC_LOG(LS_VERBOSE)
              << "Duplicate rtp header extension id " << id << ". Overwriting.";
        }

        size_t offset =
            extension_offset + extensions_size_ + extension_header_length;
        if (!rtc::IsValueInRangeForNumericType<uint16_t>(offset)) {
          RTC_DLOG(LS_WARNING) << "Oversized rtp header extension.";
          break;
        }
        extension_info.offset = static_cast<uint16_t>(offset);
        extension_info.length = length;
        extensions_size_ += extension_header_length + length;
      }
    }
    payload_offset_ = extension_offset + extensions_capacity;
  }

  if (has_padding && payload_offset_ < size) {
    padding_size_ = buffer[size - 1];
    if (padding_size_ == 0) {
      RTC_LOG(LS_WARNING) << "Padding was set, but padding size is zero";
      return false;
    }
  } else {
    padding_size_ = 0;
  }

  if (payload_offset_ + padding_size_ > size) {
    return false;
  }
  payload_size_ = size - payload_offset_ - padding_size_;
  return true;
}

const RtpPacket::ExtensionInfo* RtpPacket::FindExtensionInfo(int id) const {
  for (const ExtensionInfo& extension : extension_entries_) {
    if (extension.id == id) {
      return &extension;
    }
  }
  return nullptr;
}

RtpPacket::ExtensionInfo& RtpPacket::FindOrCreateExtensionInfo(int id) {
  for (ExtensionInfo& extension : extension_entries_) {
    if (extension.id == id) {
      return extension;
    }
  }
  extension_entries_.emplace_back(id);
  return extension_entries_.back();
}

rtc::ArrayView<const uint8_t> RtpPacket::FindExtension(ExtensionType type) const {
    uint8_t id = extensions_.GetId(type);
    if (id == ExtensionManager::kInvalidId) {
      // Extension not registered.
      return nullptr;
    }
    ExtensionInfo const* extension_info = FindExtensionInfo(id);
    if (extension_info == nullptr) {
      return nullptr;
    }
    return rtc::MakeArrayView(data() + extension_info->offset,
                            extension_info->length);
}

bool RtpPacket::HasExtension(ExtensionType type) const {
  uint8_t id = extensions_.GetId(type);
  if (id == ExtensionManager::kInvalidId) {
    // Extension not registered.
    return false;
  }
  return FindExtensionInfo(id) != nullptr;
}

} // end namespace xrtc