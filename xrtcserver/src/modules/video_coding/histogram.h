
#ifndef  __MODULES_VIDEO_CODING_HISTOGRAM_H_
#define  __MODULES_VIDEO_CODING_HISTOGRAM_H_

#include <stdint.h>
#include <vector>

namespace xrtc {

class Histogram {
public:
    // (桶的个数，最大的样本数)
    Histogram(size_t num_buckets, size_t max_num_packets);
    ~Histogram();
    
    void Add(size_t value); 
    size_t InverseCdf(float probability) const;
    size_t NumValues() const;

private:
    std::vector<size_t> values_; // 保存样本的原始数据
    std::vector<size_t> buckets_; // 存放的是每个样本出现的次数
    size_t index_ = 0; // 用了判断values_容量是否已经满了，满了就将新数据去覆盖旧数据
};

} // namespace xrtc


#endif  //__MODULES_VIDEO_CODING_HISTOGRAM_H_
