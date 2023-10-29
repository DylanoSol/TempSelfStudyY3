#pragma once
#include <iostream>
#include <thread>
#include <mutex>
#include <span>
#include <format>
#include "Globals.h"
#include "Task.h"
#include "Timing.h"
#include "Timer.h"

namespace preassigned
{
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

		size_t m_index = 0;

	};

	class Worker
	{
	public:
		Worker(ControlObject* control) : m_PControl{ control }, m_thread{ &Worker::Run, this }
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
				if constexpr (ChunkMeasurementEnabled)
				{
					m_heavyItemsProcessed += task.heavy ? 1 : 0;
				}
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

				if constexpr (ChunkMeasurementEnabled)
				{
					localTimer.StartTimer();
				}
				ProcessData_(); //Mutex remains locked when processing this. 

				if constexpr (ChunkMeasurementEnabled)
				{
					m_workTime = localTimer.GetTime();
				}

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
			if constexpr (ChunkMeasurementEnabled)
			{
				chunkTimer.StartTimer();
			}
			for (size_t iSubset = 0; iSubset < WORKER_COUNT; iSubset++)
			{
				workerPtrs[iSubset]->SetJob(std::span{&chunk[iSubset * SUBSET_SIZE], SUBSET_SIZE});
			}
			mControl.WaitForAllDone(); //This guy will wake up when all jobs are done. 
			if constexpr (ChunkMeasurementEnabled)
			{
				const auto chunkTime = chunkTimer.GetTime();
				timings.push_back({});
				for (size_t i = 0; i < WORKER_COUNT; i++)
				{
					timings.back().numberOfHeavyItemsPerThread[i] = workerPtrs[i]->GetNumHeavyItemsProcessed();
					timings.back().timeSpentWorkingPerThread[i] = workerPtrs[i]->GetJobWorkTime();
					timings.back().totalChunkTime = chunkTime;
				}
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
		if constexpr (ChunkMeasurementEnabled)
		{
			WriteCSV(timings);
		}

		return 0;
	}
}