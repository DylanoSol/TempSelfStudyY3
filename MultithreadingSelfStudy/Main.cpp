#include <iostream>
#include <vector>
#include <random>
#include <ranges> 
#include <limits>
#include <thread>
#include <mutex>
#include <algorithm>
#include <numeric>
#include <numbers>
#include <functional>
#include <sstream>
#include <condition_variable>
#include <deque>
#include "Timing.h"
#include "Globals.h"
#include "Task.h"
#include "Timer.h"
#include "Preassigned.h"
#include "Queued.h"
#include "AtomicQueued.h"

namespace tk
{
    using Task = std::function<void()>; 


    class ThreadPool
    {
    public: 
        ThreadPool(size_t numWorkers)
        {
            m_workers.reserve(numWorkers); 
            for (size_t i = 0; i < numWorkers; i++)
            {
                m_workers.emplace_back(this); 
            }
        }
        void Run(Task task)
        {
            {
                std::lock_guard lk {m_taskQueueMtx};
                m_tasks.push_back(std::move(task));
            } //We want to release the mutex before notifying the condition variable. 
            m_cv.notify_one(); 
        }

        Task GetTask(std::stop_token& st)
        {
            //This is done among a single mutex. If something locks a mutex, it will be unavailable to all other things that have access to the mutex. 
            Task task; 
            std::unique_lock lk {m_taskQueueMtx}; 
            m_cv.wait(lk, st, [this] {return !m_tasks.empty(); });
            if (!st.stop_requested())
            {
                task = std::move(m_tasks.front()); 
                m_tasks.pop_front(); 
                if (m_tasks.empty())
                {
                    m_AllDonecv.notify_all(); //Notify all the people waiting for this condition. 
                }
            }
            return task; //We can check for empty task in the call. 
        }

        void WaitForAllDone()
        {
            std::unique_lock lk {m_taskQueueMtx}; 
            m_AllDonecv.wait(lk, [this] {return m_tasks.empty(); }); //Block when task queue is empty.
        }

        ~ThreadPool()
        {
            for (auto& w : m_workers)
            {
                w.RequestStop(); 
            }
        }

    private: 

        class Worker
        {
        public:
            Worker(ThreadPool* pool) : m_PPool{ pool }, m_thread(std::bind_front(&Worker::RunKernel, this))
            {

            }
            void RequestStop()
            {
                m_thread.request_stop(); 
            }

        private:
            void RunKernel(std::stop_token st) //Jthread thing
            {
               
                while (auto task = m_PPool->GetTask(st))
                {
                    task(); 
                }
     
            }
            ThreadPool* m_PPool; 
            std::jthread m_thread;
        };

        //Data
        std::mutex m_taskQueueMtx; 
        std::condition_variable_any m_cv; 
        std::condition_variable_any m_AllDonecv;
        std::deque<Task> m_tasks; 
        std::vector<Worker> m_workers; 

    };
}

enum Datasets
{
    STACKED,
    EVENLY,
    RANDOM
};
//Following along with video tutorial series by ChiliTomatoNoodle

int main(int argc, char** argv)
{
    using namespace std::chrono_literals; 
    tk::ThreadPool pool(WORKER_COUNT); 

    const auto spitt = []
    {
        std::this_thread::sleep_for(500ms);
        std::ostringstream ss;
        ss << std::this_thread::get_id();
        std::cout << std::format("<< {} >> ", ss.str()) << std::flush;
       
    };

    for (int i = 0; i < 32; i++)
    {
        pool.Run(spitt); 
    }

    pool.WaitForAllDone(); 

    return 0; 
    /*
    Datasets run = Datasets::STACKED; 

    // generate dataset
    std::vector<std::array<Task, CHUNK_SIZE>> data;
    if (run == Datasets::STACKED) {
        data = GenerateDatasetsStacked();
    }
    else if (run == Datasets::EVENLY) {
        data = GenerateDatasetsEvenly();
    }
    else {
        data = GenerateDatasetsRandom();
    }

    // run experiment
    return AtomicQueued::DoExperiment(data); */
}
