#pragma once
#include <chrono>
class Timer
{
public: 
	Timer(); 
	void StartTimer(); 
	float GetTime(); 
private: 
	std::chrono::high_resolution_clock::time_point m_start;
	std::chrono::microseconds m_totalTime = {};
	std::chrono::duration<double> m_timeSpan;
};

