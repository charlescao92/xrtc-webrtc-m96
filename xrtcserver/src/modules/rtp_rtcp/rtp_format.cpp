#include "modules/rtp_rtcp/rtp_format.h"

#include "modules/rtp_rtcp/rtp_format_h264.h"

namespace xrtc {

    std::unique_ptr<RtpPacketizer> RtpPacketizer::Create(webrtc::VideoCodecType type,
        rtc::ArrayView<const uint8_t> payload,
        const RtpPacketizer::Config& config)
    {
        switch (type) {
        case webrtc::kVideoCodecH264:
            return std::make_unique<RtpPacketizerH264>(payload, config);
        default:
            return nullptr;
        }
    }

    std::vector<int> RtpPacketizer::SplitAboutEqual(size_t payload_size,
        const PayloadLimits& limits)
    {
        std::vector<int> result;
        // �����㹻
        if (limits.max_payload_len >= payload_size + limits.single_packet_reduction_len) {
            result.push_back(payload_size);
            return result;
        }

        // ����̫С
        if (limits.max_payload_len - limits.first_packet_reduction_len < 1 ||
            limits.max_payload_len - limits.last_packet_reduction_len < 1)
        {
            return result;
        }

        // ��Ҫ���ֵ����ֽ���
        size_t total_bytes = payload_size + limits.first_packet_reduction_len
            + limits.last_packet_reduction_len;
        // ���������Ӧ�÷�����ٸ������ʣ�����ȡ��
        size_t num_packets_left = (total_bytes + limits.max_payload_len - 1) /
            limits.max_payload_len;
        if (num_packets_left == 1) {
            num_packets_left = 2;
        }

        // ����ÿһ��������ֽ���
        size_t bytes_per_packet = total_bytes / num_packets_left;
        // ������ж��ٸ�������������1���ֽ�
        size_t num_larger_packet = total_bytes % num_packets_left;

        int remain_data = payload_size;
        bool first_packet = true;
        while (remain_data > 0) {
            // ʣ��İ���Ҫ�����һ���ֽ�
            // total_bytes 5
            // ����ĸ���3����
            // 5 / 3 = 1, 1 2 2 11 3
            // 5 % 3 = 2
            if (num_packets_left == num_larger_packet) {
                ++bytes_per_packet;
            }
            int current_packet_bytes = bytes_per_packet;

            // ���ǵ�һ�����Ĵ�С
            if (first_packet) {
                if (current_packet_bytes - limits.first_packet_reduction_len > 1) {
                    current_packet_bytes -= limits.first_packet_reduction_len;
                }
                else {
                    current_packet_bytes = 1;
                }
            }

            // ��ʣ�����ݲ���ʱ����Ҫ���⿼��
            if (current_packet_bytes > remain_data) {
                current_packet_bytes = remain_data;
            }

            // ȷ�����һ�������ܹ��ֵ�����
            if (num_packets_left == 2 && current_packet_bytes == remain_data) {
                --current_packet_bytes;
            }

            remain_data -= current_packet_bytes;
            num_packets_left--;
            result.push_back(current_packet_bytes);
            first_packet = false;
        }

        return result;
    }

} // namespace xrtc