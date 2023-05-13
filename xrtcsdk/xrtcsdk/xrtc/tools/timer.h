#ifndef KRTCSDK_KRTC_TOOLS_TIMER_H_
#define KRTCSDK_KRTC_TOOLS_TIMER_H_

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <functional>

class CTimer
{
public:
	typedef std::function<void()> OnTask;

	CTimer(unsigned int milliseconds, bool repeat, OnTask func);

	virtual ~CTimer();

	void Start();
	void Stop();
	void SetExit(bool b_exit);

private:
	void Run();

private: 
	unsigned int milliseconds_ = 1000;
	bool repeat_ = false;
	OnTask func_;

    std::atomic_bool immediately_run_{false}; // 是否立即执行
    std::atomic_bool exit_{false};
	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

#endif // KRTCSDK_KRTC_TOOLS_TIMER_H_
