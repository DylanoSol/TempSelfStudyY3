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
#include "Timing.h"
#include "Globals.h"
#include "Task.h"
#include "Timer.h"
#include "Preassigned.h"
#include "Queued.h"

using namespace preassigned; 

enum Datasets
{
    STACKED,
    EVENLY,
    RANDOM
};
//Following along with video tutorial series by ChiliTomatoNoodle

int main(int argc, char** argv)
{
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
    return queued::DoExperiment(data);
}
