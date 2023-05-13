#include <rtc_base/helpers.h>

#include "ice/ice_def.h"
#include "ice/ice_credentials.h"

namespace xrtc {

IceParameters IceCredentials::create_random_ice_credentials() {
    return IceParameters(rtc::CreateRandomString(ICE_UFRAG_LENGTH),
            rtc::CreateRandomString(ICE_PWD_LENGTH));
}

}
