#include "timer.h"

CTimer::CTimer(unsigned int milliseconds, bool repeat, CTimer::OnTask func) :
	milliseconds_(milliseconds),
	repeat_(repeat),
	func_(func) {
}

CTimer::~CTimer() {}

// 启动函数
void CTimer::Start()
{
	if (milliseconds_ == 0 || milliseconds_ == static_cast<unsigned int>(-1)) // 间隔时间为0或默认无效值，直接返回
	{
		return;
	}
	exit_.store(false);
	immediately_run_.store(false);
	thread_ = std::thread(std::bind(&CTimer::Run, this));
}

void CTimer::Stop()
{
	exit_.store(true);
	cond_.notify_all(); // 唤醒线程
	if (thread_.joinable())
	{
		thread_.join();
	}
}

void CTimer::SetExit(bool b_exit)
{
	exit_.store(b_exit);
}

void CTimer::Run()
{
	while (!exit_.load())
	{
		{
			std::unique_lock<std::mutex> locker(mutex_);

			// 如果是被唤醒的，需要判断先条件确定是不是虚假唤醒
			// wait_for是等待第三个参数满足条件，当不满足时，超时后继续往下执行
			cond_.wait_for(locker, std::chrono::milliseconds(milliseconds_), [this]()
				{ return exit_.load(); }
			);
		}

		if (exit_.load()) // 再校验下退出标识
		{
			return;
		}

		if (func_)
		{
			func_();

			if (!repeat_) {
				SetExit(true);
			}
		}
	}
}
