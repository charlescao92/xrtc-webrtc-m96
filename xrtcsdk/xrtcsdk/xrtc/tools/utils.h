#pragma once

#include <string>
#include <atomic>

#include "xrtc/tools/uuid4.h"

class Tools {
public:
    std::string RandString(uint32_t len);

private:
    void Init();
    std::string GetUUID();

private:
   bool isInit = false;

};