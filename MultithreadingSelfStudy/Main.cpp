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
#include <condition_variable>
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
        void Run(Task task)
        {
            if (auto i = std::ranges::find_if(m_workers, [](const auto& w) {return !w->IsBusy(); }); i != m_workers.end())
            {
                (*i)->Run(std::move(task)); 
            }
            else
            {
                m_workers.push_back(std::make_unique<Worker>()); 
                m_workers.back()->Run(std::move(task)); 
            }
        }

        bool IsRunningTasks()
        {
           return std::ranges::any_of(m_workers, [](const auto& w) {return w->IsBusy();  }); 
        }

    private: 

        class Worker
        {
        public:
            Worker() : m_thread(&Worker::RunKernel, this)
            {

            }

            bool IsBusy() const
            {
                return m_busy;
            }
            void Run(Task task)
            {
                std::unique_lock lk {m_mtx}; 
                m_task = std::move(task);
                m_busy = true;
                m_cv.notify_one();
            }

        private:
            void RunKernel() //Jthread thing
            {
                std::unique_lock lk {m_mtx};
                auto st = m_thread.get_stop_token();
                while (m_cv.wait(lk, st, [this]() -> bool {return m_busy; }))
                {
                    
                    //Wakeup point
                    if (st.stop_requested())
                    {
                        return;
                    }
                    m_task();
                    m_task = {}; //Empty the task
                    m_busy = false;
                }
            }

            std::atomic<bool> m_busy = false;
            std::condition_variable_any m_cv;
            std::mutex m_mtx;
            Task m_task;
            std::jthread m_thread;
        };
        std::vector<std::unique_ptr<Worker>> m_workers; 

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
    tk::ThreadPool pool; 
    pool.Run([] { std::cout << "Hi" << std::endl; });
    pool.Run([] { std::cout << "Ho" << std::endl; });

    while (pool.IsRunningTasks())
    {
        using namespace std::chrono_literals; 
        std::this_thread::sleep_for(16ms); 
    }
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
