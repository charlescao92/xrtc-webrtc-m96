#include <modules/rtp_rtcp/source/byte_io.h>

#include "modules/rtp_rtcp/rtp_format_h264.h"
#include "modules/rtp_rtcp/rtp_packet_to_send.h"

namespace xrtc {

	const size_t kNaluShortStartSequenceSize = 3;
	const size_t kFuAHeaderSize = 2;
	const size_t kNaluHeaderSize = 1;
	const size_t kLengthFieldSize = 2;

	// NALͷ����bit mask
	enum NalDef : uint8_t {
		kFBit = 0x80,
		kNriMask = 0x60,
		kTypeMask = 0x1F,
	};

	// FU-Header bit mask
	enum FuDef : uint8_t {
		kSBit = 0x80,
		kEBit = 0x40,
		kRBit = 0x20,
	};

	RtpPacketizerH264::RtpPacketizerH264(rtc::ArrayView<const uint8_t> payload, 
		const RtpPacketizer::Config& config):
		config_(config)
	{
		for (const auto& nalu : FindNaluIndices(payload.data(), payload.size())) {
			input_fragments_.push_back(payload.subview(nalu.payload_start_offset, nalu.payload_size));
		}

		if (!GeneratePackets()) {
			num_packets_left_ = 0;
			while (!packets_.empty()) {
				packets_.pop();
			}
		}
	}

	size_t RtpPacketizerH264::NumPackets() {
		return num_packets_left_;
	}

	bool RtpPacketizerH264::NextPacket(RtpPacketToSend* rtp_packet) {
		if (packets_.empty()) {
			return false;
		}

		PacketUnit* packet = &packets_.front();
		if (packet->first_fragment && packet->last_fragment) {
			// ����NALU��
			size_t packet_size = packet->source_fragment.size();
			uint8_t* buffer = rtp_packet->AllocatePayload(packet_size);
			memcpy(buffer, packet->source_fragment.data(), packet_size);
			packets_.pop();
			input_fragments_.pop_front();
		}
		else if (packet->aggregated) {
			// STAP-A
			NextAggregatedPacket(rtp_packet);
		}
		else {
			// FU-A
			NextFragmentPacket(rtp_packet);
		}

		--num_packets_left_;
		rtp_packet->SetMarker(packets_.empty());

		return true;
	}

	std::vector<NaluIndex> RtpPacketizerH264::FindNaluIndices(const uint8_t* buffer, 
		size_t buffer_size)
	{
		// ��ʼ����3������4���ֽ�
		std::vector<NaluIndex> sequences;
		if (buffer_size < kNaluShortStartSequenceSize) {
			return sequences;
		}

		size_t end = buffer_size - kNaluShortStartSequenceSize;
		for (size_t i = 0; i < end;) {
			// ����NALU����ʼ��
			if (buffer[i + 2] > 1) {
				i += 3;
			}
			else if (buffer[i + 2] == 1) { // �п�������ʼ��
				if (buffer[i] == 0 && buffer[i + 1] == 0) {
					// �ҵ���һ����ʼ��
					NaluIndex index = { i, i + 3, 0 }; // ��ʱ��0���Ȼ�ȡ��һ����ʼ��λ�ú�����ȥ��ǰ��ʼ��ĩβλ��
					 // �Ƿ���4�ֽڵ���ʼ��
					if (index.start_offset > 0 && buffer[index.start_offset - 1] == 0) {
						--index.start_offset;
					}

					auto it = sequences.rbegin();
					if (it != sequences.rend()) {
						it->payload_size = index.start_offset - it->payload_start_offset;
					}

					sequences.push_back(index);
				}
				i += 3;
			}
			else if (buffer[i + 2] == 0) {
				i += 1;
			}
		}

		// �������һ��NALU��payload size
		auto it = sequences.rbegin();
		if (it != sequences.rend()) {
			it->payload_size = buffer_size - it->payload_start_offset;
		}

		return sequences;
	}

	bool RtpPacketizerH264::GeneratePackets() {
		// ������buffer������ȡ��NALU
		for (size_t i = 0; i < input_fragments_.size(); ) {
			size_t fragment_len = input_fragments_[i].size();
			// 1�����Ȼ�ȡ��NALU���ɸ��ص��������
			size_t single_packet_capacity = config_.limits.max_payload_len;
			if (input_fragments_.size() == 1) { // ֻ��һ��NALU�����
				single_packet_capacity -= config_.limits.single_packet_reduction_len;
			}
			else if (i == 0) { // ��һ����
				single_packet_capacity -= config_.limits.first_packet_reduction_len;
			}
			else if (i + 1 == input_fragments_.size()) {  // ���һ����
				single_packet_capacity -= config_.limits.last_packet_reduction_len;
			}

			// �Ƚϴ�С��ʹ�ò�ͬ�Ĵ����ʽ
			if (fragment_len > single_packet_capacity) { // ��Ƭ���
				if (!PacketizeFuA(i)) {
					return false;
				}
				++i;
			}
			else {
				i = PacketizeStapA(i);
			}

		}
		return true;
	}

	// ��Ƭ���
	bool RtpPacketizerH264::PacketizeFuA(size_t fragment_index) {
		rtc::ArrayView<const uint8_t> fragment = input_fragments_[fragment_index];
		PayloadLimits limits = config_.limits;
		// Ԥ��FU-Aͷ���Ŀռ�
		limits.max_payload_len -= kFuAHeaderSize;
		// ����Ƕ��NALU
		if (input_fragments_.size() != 1) {
			if (fragment_index == input_fragments_.size() - 1) {
				// ������ֻ������м�İ������һ����
				limits.single_packet_reduction_len = limits.last_packet_reduction_len;
			}
			else if (fragment_index == 0) {
				// ������ֻ������һ�������м��
				limits.single_packet_reduction_len = limits.first_packet_reduction_len;
			}
			else {
				// ֻ�����м��
				limits.single_packet_reduction_len = 0;
			}
		}

		if (fragment_index != 0) {
			// ��һ���������ܰ��������NALU
			limits.first_packet_reduction_len = 0;
		}

		if (fragment_index != input_fragments_.size() - 1) {
			// ���һ���������ܳ��������NALU
			limits.last_packet_reduction_len = 0;
		}

		size_t payload_left = fragment.size() - kNaluHeaderSize;
		// ���ص���ʼƫ����
		size_t offset = kNaluHeaderSize;
		// �����ش�С�ָ�ɴ�����ͬ�ļ�������
		std::vector<int> payload_sizes = SplitAboutEqual(payload_left, limits);
		if (payload_sizes.empty()) {
			return false;
		}

		for (size_t i = 0; i < payload_sizes.size(); ++i) {
			size_t packet_length = payload_sizes[i];
			packets_.push(PacketUnit(
				fragment.subview(offset, packet_length),
				i == 0,
				i == payload_sizes.size() - 1,
				false, fragment[0]
			));

			offset += packet_length;
		}

		num_packets_left_ += payload_sizes.size();

		return true;
	}

	size_t RtpPacketizerH264::PacketizeStapA(size_t fragment_index) {
		size_t payload_size_left = config_.limits.max_payload_len;
		if (input_fragments_.size() == 1) {
			payload_size_left -= config_.limits.single_packet_reduction_len;
		}
		else if (fragment_index == 0) {
			// ��һ����
			payload_size_left -= config_.limits.first_packet_reduction_len;
		}

		int aggregated_fragment = 0;
		int fragment_header_length = 0;
		rtc::ArrayView<const uint8_t> fragment = input_fragments_[fragment_index];
		++num_packets_left_;

		auto payload_size_needed = [&] {
			size_t fragment_size = fragment.size() + fragment_header_length;
			if (input_fragments_.size() == 1) {
				return fragment_size;
			}

			if (fragment_index == input_fragments_.size() - 1) {
				return fragment_size + config_.limits.last_packet_reduction_len;
			}

			return fragment_size;
		};

		while (payload_size_left >= payload_size_needed()) {
			packets_.push(PacketUnit(
				fragment,
				aggregated_fragment == 0,
				false,
				true,
				fragment[0]));

			// ����������¼���ʣ��������С
			payload_size_left -= fragment.size();
			payload_size_left -= fragment_header_length;

			// �������ϵڶ�������ʼ������Ҫ����ͷ����С��
			fragment_header_length = kLengthFieldSize;
			if (0 == aggregated_fragment) {
				fragment_header_length += (kNaluHeaderSize + kLengthFieldSize);
			}

			++aggregated_fragment;
	
			++fragment_index;
			if (fragment_index == input_fragments_.size()) {
				break;
			}

			// �����ۺ���һ����
			fragment = input_fragments_[fragment_index];

		}

		packets_.back().last_fragment = true;

		return fragment_index;
	}

	void RtpPacketizerH264::NextAggregatedPacket(RtpPacketToSend* rtp_packet) {
		size_t rtp_packet_capacity = rtp_packet->FreeCapacity();
		uint8_t* buffer = rtp_packet->AllocatePayload(rtp_packet_capacity);
		PacketUnit* packet = &packets_.front();

		// д��STAP-A header
		buffer[0] = (packet->header & (kFBit | kNriMask)) | NaluType::kStapA;

		size_t index = kNaluHeaderSize;
		bool is_last_fragment = packet->last_fragment;

		// д��NALU
		while (packet->aggregated) {
			rtc::ArrayView<const uint8_t> fragment = packet->source_fragment;
			
			// д��NALU length field
			webrtc::ByteWriter<uint16_t>::WriteBigEndian(&buffer[index], (uint16_t)fragment.size());
			index += kLengthFieldSize;
			
			// д��NALU
			memcpy(&buffer[index], fragment.data(), fragment.size());
			index += fragment.size();
			packets_.pop();
			input_fragments_.pop_front();

			if (is_last_fragment) {
				break;
			}

			packet = &packets_.front();
			is_last_fragment = packet->last_fragment;
		}

		rtp_packet->SetPayloadSize(index);
	}

	void RtpPacketizerH264::NextFragmentPacket(RtpPacketToSend* rtp_packet) {
		PacketUnit* packet = &packets_.front();

		// ����FU-Indicator
		uint8_t fu_indicator = (packet->header & (kFBit | kNriMask)) | NaluType::kFuA;

		// ����FU-Header
		uint8_t fu_header = 0;
		fu_header |= (packet->first_fragment ? kSBit : 0);
		fu_header |= (packet->last_fragment ? kEBit : 0);

		// ��ȡԭʼ��NALU type
		uint8_t type = packet->header & kTypeMask;
		fu_header |= type;

		// д�뵽rtp buffer
		rtc::ArrayView<const uint8_t> fragment = packet->source_fragment;
		uint8_t* buffer = rtp_packet->AllocatePayload(kFuAHeaderSize + fragment.size());
		buffer[0] = fu_indicator;
		buffer[1] = fu_header;
		memcpy(buffer + kFuAHeaderSize, fragment.data(), fragment.size());
		
		packets_.pop();

		if (packet->last_fragment) {
			input_fragments_.pop_front();
		}
	}

} // end namespace xrtc
