#ifndef MODULES_RTP_RTCP_RTP_FORMAT_H264_H_
#define MODULES_RTP_RTCP_RTP_FORMAT_H264_H_

#include <vector>
#include <deque>
#include <queue>

#include <api/array_view.h>

#include "modules/rtp_rtcp/rtp_format.h"

namespace xrtc {

    struct NaluIndex {
        size_t start_offset;         // NALU����ʼλ�ã�������ʼ��
        size_t payload_start_offset; // NALU���ص���ʼλ��
        size_t payload_size;         // NALU���ش�С
    };

    enum NaluType : uint8_t {
        kSlice = 1,
        kIdr = 5,
        kSei = 6,
        kSps = 7,
        kPps = 8,
        kStapA = 24,
        kFuA = 28,
    };

    class RtpPacketizerH264 : public RtpPacketizer {
    public:
        RtpPacketizerH264(rtc::ArrayView<const uint8_t> payload,
            const RtpPacketizer::Config& config);
        ~RtpPacketizerH264() override = default;

        size_t NumPackets() override;
        bool NextPacket(RtpPacketToSend* rtp_packet) override;

    private:
        struct PacketUnit {
            PacketUnit(rtc::ArrayView<const uint8_t> source_fragment,
                bool first_fragment,
                bool last_fragment,
                bool aggregated,
                uint8_t header) :
                source_fragment(source_fragment),
                first_fragment(first_fragment),
                last_fragment(last_fragment),
                aggregated(aggregated),
                header(header) {}

            rtc::ArrayView<const uint8_t> source_fragment;
            bool first_fragment;
            bool last_fragment;
            bool aggregated;
            uint8_t header;
        };

        std::vector<NaluIndex> FindNaluIndices(const uint8_t* buffer,
            size_t buffer_size);
        bool GeneratePackets();
        bool PacketizeFuA(size_t fragment_index);
        size_t PacketizeStapA(size_t fragment_index);
        void NextAggregatedPacket(RtpPacketToSend* rtp_packet);
        void NextFragmentPacket(RtpPacketToSend* rtp_packet);

    private:
        RtpPacketizer::Config config_;
        std::deque<rtc::ArrayView<const uint8_t>> input_fragments_;
        std::queue<PacketUnit> packets_;
        size_t num_packets_left_ = 0;
    };

} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTP_FORMAT_H264_H_