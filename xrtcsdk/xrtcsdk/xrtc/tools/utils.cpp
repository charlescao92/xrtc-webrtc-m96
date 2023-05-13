#include "xrtc/tools/utils.h"

#include <assert.h>

#define CHECK_TOOLS_INIT \
  if (!isInit) {         \
    Init();              \
  }

void Tools::Init() {
    srand(time(nullptr));
    auto ret = uuid4_init();
    assert(ret == UUID4_ESUCCESS);
    isInit = true;
}

std::string Tools::GetUUID() {
    CHECK_TOOLS_INIT
        char uuid[UUID4_LEN] = { 0 };
    uuid4_generate(uuid);
    return uuid;
}

std::string Tools::RandString(uint32_t len) {
    std::string uuid = GetUUID();
    for (auto& u : uuid) {
        if (u == '-') {
            u = 'a';
        }
    }
    if (len < uuid.size()) {
        return uuid.substr(0, len);
    }
    else {
        std::string str;
        for (uint32_t i = 0; i < len - uuid.size(); i++) {
            uuid += rand() % 26 + 'a';
        }
    }
    return uuid;
}
