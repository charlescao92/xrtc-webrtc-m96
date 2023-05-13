#include "pc/stream_params.h"

namespace xrtc {

SsrcGroup::SsrcGroup(const std::string& semantics, const std::vector<uint32_t>& ssrcs) :
    semantics(semantics), ssrcs(ssrcs) {}

bool StreamParams::has_ssrc(uint32_t ssrc) {
    for (auto item : ssrcs) {
        if (item == ssrc) {
            return true;
        }
    }
    return false;
}

} // end namespace xrtc
