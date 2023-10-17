#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <ranges> 
#include <limits>
#include <thread>
#include <mutex>
#include <span>
#include <algorithm>
#include <numeric>
#include "Timer.h"

//Settings for now
constexpr size_t WORKER_COUNT = 4; 
constexpr size_t CHUNK_SIZE = 100;
constexpr size_t CHUNK_COUNT = 100;
constexpr size_t SUBSET_SIZE = CHUNK_SIZE / WORKER_COUNT; 
constexpr size_t LIGHT_ITERATIONS = 100;
constexpr size_t HEAVY_ITERATIONS = 1000;
constexpr double ProbabilityHeavy = .02; 

static_assert(CHUNK_SIZE >= WORKER_COUNT); 
static_assert(CHUNK_SIZE % WORKER_COUNT == 0);

struct Task
{
	unsigned int val; 
	bool heavy; 
	unsigned int Process() const
	{
		const auto iterations = heavy ? HEAVY_ITERATIONS : LIGHT_ITERATIONS; 
		double intermediate = 2. * double(val) / double(std::numeric_limits<unsigned int>::max()) - 1; 
		for (size_t i = 0; i < iterations; i++)
		{
			intermediate = std::sin(std::cos(intermediate)); 
		}
		return unsigned int((intermediate + 1.) * 0.5 * double(std::numeric_limits<unsigned int>::max())); 
	}
};

//Following along with video tutorial series by ChiliTomatoNoodle
class ControlObject
{
public:
	ControlObject() : m_lk{ m_mtx }
	{

	}

	void SignalDone()
	{
		bool needsNotification = false; 
		{
			std::lock_guard lk {m_mtx}; 
			m_doneCount++; 

			//Has to happen while the mutex is still held, so there's no race condition
			if (m_doneCount == WORKER_COUNT)
			{
				//Notify the condition variable of this thread. 
				needsNotification = true; 
			}
		}
		if (needsNotification)
		{
			m_cv.notify_one();
		}
	}

	void WaitForAllDone()
	{
		//Wait until work is done. 
		m_cv.wait(m_lk, [this] {return m_doneCount == WORKER_COUNT; }); 
		m_doneCount = 0; 
	}

private:
	std::condition_variable m_cv; 
	std::mutex m_mtx; 
	std::unique_lock<std::mutex> m_lk; 
	//SharedMemory 
	int m_doneCount = 0; 

};

class Worker
{
public:
	Worker(ControlObject* control) : m_PControl{ control }, m_thread{&Worker::Run, this} 
	{

	}

	//Function will be called by the main thread. 
	void SetJob(std::span<const Task> data)
	{
		{
			std::lock_guard lk {m_mtx}; 
			m_input = data; 
		}

		m_cv.notify_one(); 
	}

	void Kill()
	{
		{
			std::lock_guard lk {m_mtx};
			m_threadDying = true; 
		}

		m_cv.notify_one();
	}

	double GetResult() const
	{
		return m_accumulate; 
	}

private:
	void ProcessData_()
	{
		for (const auto& task : m_input)
		{
			m_accumulate += task.Process(); 
		}
	}

	//Run is the while loop happening on the thread. The other functions here are interface functions abstracted, and happen from the main thread. 
	void Run()
	{
		std::unique_lock lk {m_mtx}; 
		while (true)
		{
			//1. Lock. 2. Extra condition. 
			m_cv.wait(lk, [this] {return !m_input.empty() || m_threadDying; }); 
			if (m_threadDying) break; 
			ProcessData_(); //Mutex remains locked when processing this. 
			m_input = {}; //Zero out input. 
			m_PControl->SignalDone(); 
		}
	}

	ControlObject* m_PControl; 
	std::jthread m_thread; 
	std::condition_variable m_cv; 
	std::mutex m_mtx; 

	//Shared memory. 
	std::span<const Task> m_input; 
	unsigned int m_accumulate = 0; 
	bool m_threadDying = false; 
};




std::vector<std::array<Task, CHUNK_SIZE>> GenerateDatasets()
{
	std::minstd_rand randomNumberEngine;
	std::uniform_real_distribution<double> dist{-1., 1}; 
	std::bernoulli_distribution dist2 {ProbabilityHeavy}; 
	std::vector<std::array<Task, CHUNK_SIZE>> Chunks(CHUNK_COUNT);

	for (auto& chunk : Chunks)
	{
		//Generate random ranges. Just make this long
		std::ranges::generate(chunk, [&] { return Task{ .val = randomNumberEngine(), .heavy = dist2(randomNumberEngine)};  });
	}

	return Chunks; 
}

int DoExperiment()
{
	const auto chunks = GenerateDatasets(); 

	Timer timer;
	timer.StartTimer();

	ControlObject mControl;
	std::vector<std::unique_ptr<Worker>> workerPtrs; 
	for (size_t i = 0; i < WORKER_COUNT; i++)
	{
		workerPtrs.push_back(std::make_unique<Worker>(&mControl)); 
	}

	for (const auto& chunk : chunks)
	{
		for (size_t iSubset = 0; iSubset < WORKER_COUNT; iSubset++)
		{
			workerPtrs[iSubset]->SetJob(std::span{&chunk[iSubset * SUBSET_SIZE], SUBSET_SIZE});
		}
		mControl.WaitForAllDone(); //This guy will wake up when all jobs are done. 
	}

	float timeElapsed = timer.GetTime();
	printf("%f milliseconds \n", timeElapsed);
	unsigned int answer = 0.; 
	for (const auto& w : workerPtrs)
	{
		answer += w->GetResult(); 
	}
	std::cout << "Result is " << answer << std::endl;

	for (auto& w : workerPtrs)
	{
		w->Kill(); 
	}

	return 0; 

}

int main(int argc, char** argv)
{
	
	//return SmallOperation(); 
	//return BigOperation(); 
	return DoExperiment(); 
}
