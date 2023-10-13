#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <ranges> 
#include <limits>
#include <thread>
#include <mutex>
#include <span>
#include "Timer.h"

void ProcessDataset(std::span<int> set, int& sum)
{
	for (int x : set)
	{
		//1 thread locks the mutex. 

		//Random heavy math operation. There just needs to be work. 
		constexpr auto limit = (double)std::numeric_limits<int>::max();
		const auto y = (double)x / limit;
		sum += int(std::sin(std::cos(y)) * limit);

		//Same thread unlocks the mutex. 

	}
}

//Following along with video tutorial series by ChiliTomatoNoodle
class ControlObject
{
public:
	ControlObject(int workerCount) : m_lk{ m_mtx }, m_workerCount{ workerCount }
	{

	}

	void SignalDone()
	{
		{
			std::lock_guard lk {m_mtx}; 
			m_doneCount++; 
		}

		if (m_doneCount == m_workerCount)
		{
			//Notify the condition variable of this thread. 
			m_cv.notify_one(); 
		}
	}

	void WaitForAllDone()
	{
		//Wait until work is done. 
		m_cv.wait(m_lk, [this] {return m_doneCount == m_workerCount; }); 
		m_doneCount = 0; 
	}

private:
	std::condition_variable m_cv; 
	std::mutex m_mtx; 
	std::unique_lock<std::mutex> m_lk; 
	int m_workerCount; 
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
	void SetJob(std::span<int> data, int* pOut)
	{
		{
			std::lock_guard lk {m_mtx}; 
			m_input = data; 
			m_pOutput = pOut; 
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

private:
	void Run()
	{
		std::unique_lock lk {m_mtx}; 
		while (true)
		{
			//1. Lock. 2. Extra condition. 
			m_cv.wait(lk, [this] {return m_pOutput != nullptr || m_threadDying; }); 
			if (m_threadDying) break; 
			ProcessDataset(m_input, *m_pOutput); //Mutex remains locked when processing this. 
			m_pOutput = nullptr; 
			m_input = {}; //Zero out input. 
			m_PControl->SignalDone(); 
		}
	}

	ControlObject* m_PControl; 
	std::jthread m_thread; 
	std::condition_variable m_cv; 
	std::mutex m_mtx; 

	//Shared memory. 
	std::span<int> m_input; 
	int* m_pOutput = nullptr; 
	bool m_threadDying = false; 
};

constexpr size_t DATASET_SIZE = 50000000; 




std::vector<std::array<int, DATASET_SIZE>> GenerateDatasets()
{
	std::minstd_rand randomNumberEngine;
	std::vector<std::array<int, DATASET_SIZE>> datasets{4};

	for (auto& arr : datasets)
	{
		//Generate random ranges. Just make this long
		std::ranges::generate(arr, randomNumberEngine);
	}

	return datasets; 
}

int BigOperation()
{
	auto datasets = GenerateDatasets(); 

	Timer timer; 
	std::vector<std::thread> workers; 


	struct Value
	{
		int v; //4 byte data type
		char padding[60]; //60 bytes to add up to 64 bytes. 
	};

	Value sum[4] = { 0, 0, 0, 0 };

	timer.StartTimer();

	for (size_t i = 0; i < 4; i++)
	{
		workers.push_back(std::thread{ProcessDataset, std::span{datasets[i]}, std::ref(sum[i].v)});
	}

	for (auto& w : workers)
	{
		w.join(); 
	}

	float timeElapsed = timer.GetTime(); 
	printf("%f milliseconds \n", timeElapsed); 
	printf("%d sum \n", sum[0].v + sum[1].v + sum[2].v + sum[3].v);

	return 0; 
}



int SmallOperation()
{

	struct Value
	{
		int v; //4 byte data type
		char padding[60]; //60 bytes to add up to 64 bytes. 
	};

	Value sum[4] = { 0, 0, 0, 0 };

	Timer timer;
	timer.StartTimer();


	constexpr size_t workerCount = 4;
	ControlObject mControl{ workerCount };
	//Unique_ptr is necessary here to prevent dangling pointers.
	std::vector<std::unique_ptr<Worker>> workerPtrs;

	for (size_t i = 0; i < workerCount; i++)
	{
		workerPtrs.push_back(std::make_unique<Worker>(&mControl)); //Pointers now going to be idling, waiting for jobs. 
	}


	auto datasets = GenerateDatasets(); 

	int grandTotal = 0; 
	std::vector<std::jthread> workers;
	const auto subsetSize = DATASET_SIZE / 10000;
	for (size_t i = 0; i < DATASET_SIZE; i += subsetSize)
	{
		for (size_t j = 0; j < 4; j++)
		{
			workerPtrs[j]->SetJob(std::span{&datasets[j][i], subsetSize}, &sum[j].v);
		}
		mControl.WaitForAllDone(); //This guy will wake up when all jobs are done. 
	}

	grandTotal = sum[0].v + sum[1].v + sum[2].v + sum[3].v;

	float timeElapsed = timer.GetTime();
	printf("%f milliseconds \n", timeElapsed);
	printf("%d sum \n", grandTotal);

	//Don't leave the threads dangling. 

	for (auto& w : workerPtrs)
	{
		w->Kill(); 
	}
	workerPtrs.clear(); 

	return 0; 
}

int main(int argc, char** argv)
{
	
	return SmallOperation(); 
	//return BigOperation(); 
}
