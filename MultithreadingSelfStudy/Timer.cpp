#include "Timer.h"

Timer::Timer()
{

}

float Timer::GetTime()
{
	std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
	m_totalTime = std::chrono::duration_cast<std::chrono::microseconds>(t2 - m_start);

	return static_cast<float>(m_totalTime.count());
}

void Timer::StartTimer()
{
	m_start = std::chrono::high_resolution_clock::now();
}