/**
 * @file utils.h
 * @author charles
 * @brief 
*/

#ifndef __BASE_UTILS_H_
#define __BASE_UTILS_H_

namespace xrtc {

template <typename T>
class Singleton {
public:
    static T* Instance() {
        static T instance;
        return &instance;
    }
};

}

#endif // __BASE_UTILS_H_
