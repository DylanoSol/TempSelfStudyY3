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

namespace queued
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

		void SetChunk(std::span<const Task> chunk)
		{
			m_index = 0; 
			m_currentChunk = chunk; 
		}

		const Task* GetTask()
		{
			const auto i = m_index++; 
			if (i >= CHUNK_SIZE)
			{
				return nullptr; 
			}
			return &m_currentChunk[i]; 
		}

	private:
		std::condition_variable m_cv;
		std::mutex m_mtx;
		std::unique_lock<std::mutex> m_lk;
		std::span<const Task> m_currentChunk; //Basically a flexible array. 
		//SharedMemory 
		int m_doneCount = 0;
		std::atomic<size_t> m_index = 0; 
	};

	class Worker
	{
	public:
		Worker(ControlObject* control) : m_PControl{ control }, m_thread{ &Worker::Run, this }
		{

		}

		void StartWork()
		{
			{
				std::lock_guard lk {m_mtx};
				m_working = true;
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

		~Worker()
		{
			Kill();
		}

	private:
		void ProcessData_()
		{

			m_heavyItemsProcessed = 0;
			while (auto pTask = m_PControl->GetTask()) //As long as there are still tasks, it will keep running. 
			{
				m_accumulate += pTask->Process();
				if constexpr (ChunkMeasurementEnabled)
				{
					m_heavyItemsProcessed += pTask->heavy ? 1 : 0;
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
				m_cv.wait(lk, [this] {return m_working || m_threadDying; }); //If working or dying, wake up. 
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

				m_working = false; 
				m_PControl->SignalDone();
			}
		}

		ControlObject* m_PControl;
		std::jthread m_thread;
		std::condition_variable m_cv;
		std::mutex m_mtx;

		//Shared memory. 
		unsigned int m_accumulate = 0;
		bool m_threadDying = false;
		float m_workTime = -1.f;
		size_t m_heavyItemsProcessed = 0;
		bool m_working = false; 
	};

	int DoExperiment(std::vector<std::array<Task, CHUNK_SIZE>> chunks)
	{
		std::vector<ChunkTimingInfo> timings;
		timings.reserve(CHUNK_COUNT);

		Timer timer;
		timer.StartTimer();

		ControlObject mControl;
		std::vector<std::unique_ptr<Worker>> workerPtrs(WORKER_COUNT);

		std::ranges::generate(workerPtrs, [m_PControl = &mControl] {return std::make_unique<Worker>(m_PControl); });

		Timer chunkTimer;

		for (const auto& chunk : chunks)
		{
			if constexpr (ChunkMeasurementEnabled)
			{
				chunkTimer.StartTimer();
			}

			mControl.SetChunk(chunk); 
			for (auto& pWorker : workerPtrs)
			{
				pWorker->StartWork(); 
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
};