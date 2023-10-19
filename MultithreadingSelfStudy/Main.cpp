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
#include <numbers>
#include <fstream>
#include <format>
#include <functional>
#include "Timer.h"

//Settings for now
constexpr size_t WORKER_COUNT = 4; 
constexpr size_t CHUNK_SIZE = 8000;
constexpr size_t CHUNK_COUNT = 100;
constexpr size_t SUBSET_SIZE = CHUNK_SIZE / WORKER_COUNT; 
constexpr size_t LIGHT_ITERATIONS = 100;
constexpr size_t HEAVY_ITERATIONS = 1000;
constexpr double ProbabilityHeavy = .15; 

static_assert(CHUNK_SIZE >= WORKER_COUNT); 
static_assert(CHUNK_SIZE % WORKER_COUNT == 0);

struct Task
{
	double val; 
	bool heavy; 
	unsigned int Process() const
	{
		const auto iterations = heavy ? HEAVY_ITERATIONS : LIGHT_ITERATIONS; 
		double intermediate = val; 
		for (size_t i = 0; i < iterations; i++)
		{
			unsigned int digits = unsigned int(std::abs(std::sin(std::cos(intermediate)) * 10000000)) % 100000; //Module slice out some digits.
			intermediate = double(digits) / 10000.;
		}
		return unsigned int(std::exp(intermediate)); 
	}
};

struct ChunkTimingInfo
{
	std::array<float, WORKER_COUNT> timeSpentWorkingPerThread; 
	std::array<size_t, WORKER_COUNT> numberOfHeavyItemsPerThread; 
	float totalChunkTime; 

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

	float GetJobWorkTime() const
	{
		return m_workTime; 
	}

	double GetResult() const
	{
		return m_accumulate; 
	}

	size_t GetNumHeavyItemsProcessed() const
	{
		return m_heavyItemsProcessed; 
	}

private:
	void ProcessData_()
	{
		m_heavyItemsProcessed = 0; 
		for (const auto& task : m_input)
		{
			m_accumulate += task.Process(); 
			m_heavyItemsProcessed += task.heavy ? 1 : 0; 
		}
	}

	//Run is the while loop happening on the thread. The other functions here are interface functions abstracted, and happen from the main thread. 
	void Run()
	{
		std::unique_lock lk {m_mtx}; 
		Timer localTimer;
		while (true)
		{
			//1. Lock. 2. Extra condition. 
			m_cv.wait(lk, [this] {return !m_input.empty() || m_threadDying; }); 
			if (m_threadDying) break; 

			localTimer.StartTimer(); 
			ProcessData_(); //Mutex remains locked when processing this. 
			m_workTime = localTimer.GetTime(); 

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
	float m_workTime = -1.f;
	size_t m_heavyItemsProcessed = 0; 
};

std::vector<std::array<Task, CHUNK_SIZE>> GenerateDatasetsEvenly()
{
	std::minstd_rand randomNumberEngine;
	std::uniform_real_distribution dist {0., std::numbers::pi};

	const int everyNth = int(1. / ProbabilityHeavy);
	std::vector<std::array<Task, CHUNK_SIZE>> Chunks(CHUNK_COUNT);

	for (auto& chunk : Chunks)
	{
		//Generate random ranges. Just make this long
		std::ranges::generate(chunk, [&, i = 0]() mutable {
			const auto isHeavy = i++ % everyNth == 0;
			return Task{ .val = dist(randomNumberEngine), .heavy = isHeavy };
			});
	}

	return Chunks;
}

std::vector<std::array<Task, CHUNK_SIZE>> GenerateDatasetsStacked()
{
	auto data = GenerateDatasetsEvenly();
	for (auto& chunk : data)
	{
		std::ranges::partition(chunk, std::identity{}, & Task::heavy);
	}

	return data;
}

std::vector<std::array<Task, CHUNK_SIZE>> GenerateDatasetsRandom()
{
	std::minstd_rand randomNumberEngine;
	std::uniform_real_distribution dist {0., std::numbers::pi};
	std::bernoulli_distribution dist2 {ProbabilityHeavy}; 
	std::vector<std::array<Task, CHUNK_SIZE>> Chunks(CHUNK_COUNT);

	for (auto& chunk : Chunks)
	{
		//Generate random ranges. Just make this long
		std::ranges::generate(chunk, [&] { return Task{ .val = dist(randomNumberEngine), .heavy = dist2(randomNumberEngine)};  });
	}

	return Chunks; 
}

int DoExperiment(bool stacked = false)
{
	const auto chunks = [=]
	{
		if (stacked)
		{
			return GenerateDatasetsStacked();
		}
		else
		{
			return GenerateDatasetsEvenly();
		}
	}(); 

	Timer timer;
	timer.StartTimer();

	ControlObject mControl;
	std::vector<std::unique_ptr<Worker>> workerPtrs; 
	for (size_t i = 0; i < WORKER_COUNT; i++)
	{
		workerPtrs.push_back(std::make_unique<Worker>(&mControl)); 
	}

	std::vector<ChunkTimingInfo> timings; 
	timings.reserve(CHUNK_COUNT); 

	Timer chunkTimer; 

	for (const auto& chunk : chunks)
	{
		chunkTimer.StartTimer(); 
		for (size_t iSubset = 0; iSubset < WORKER_COUNT; iSubset++)
		{
			workerPtrs[iSubset]->SetJob(std::span{&chunk[iSubset * SUBSET_SIZE], SUBSET_SIZE});
		}
		mControl.WaitForAllDone(); //This guy will wake up when all jobs are done. 
		const auto chunkTime = chunkTimer.GetTime(); 
		timings.push_back({});
		for (size_t i = 0; i < WORKER_COUNT; i++)
		{
			timings.back().numberOfHeavyItemsPerThread[i] = workerPtrs[i]->GetNumHeavyItemsProcessed(); 
			timings.back().timeSpentWorkingPerThread[i] = workerPtrs[i]->GetJobWorkTime(); 
			timings.back().totalChunkTime = chunkTime; 
		}
	}

	float timeElapsed = timer.GetTime();
	printf("%f microseconds \n", timeElapsed);
	unsigned int answer = 0.; 
	for (const auto& w : workerPtrs)
	{
		answer += w->GetResult(); 
	}
	std::cout << "Result is " << answer << std::endl;

	//Output csv of chunk timings. 
	// worktime, idletime, numberofheavies x workers + total time, total heavies
	std::ofstream csv{ "timings.csv" , std::ios_base::trunc}; 
	for (size_t i = 0; i < WORKER_COUNT; i++)
	{
		csv << std::format("work_{0:};idle_{0:};heavy_{0:};", i);
	}
	csv << "chunktime,total_idle,total_heavy\n"; 

	for (const auto& chunk : timings)
	{
		float totalIdle = 0.f; 
		size_t totalHeavy = 0; 
		for (size_t i = 0; i < WORKER_COUNT; i++)
		{
			double idle = chunk.totalChunkTime - chunk.timeSpentWorkingPerThread[i];
			double heavy = chunk.numberOfHeavyItemsPerThread[i]; 

			csv << std::format("{};{};{};", chunk.timeSpentWorkingPerThread[i], idle, heavy);
			totalIdle += idle; 
			totalHeavy += heavy; 
		}
		csv << std::format("{};{};{}\n", chunk.totalChunkTime, totalIdle, totalHeavy);
	}


	for (auto& w : workerPtrs)
	{
		w->Kill(); 
	}

	return 0; 

}

int main(int argc, char** argv)
{
	return DoExperiment(true); 
}
