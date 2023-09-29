#include <iostream>
#include <vector>
#include <array>
#include <random>
#include <ranges> 
#include <limits>
#include <thread>
#include <mutex>
#include "Timer.h"

//Following along with video tutorial series by ChiliTomatoNoodle
constexpr size_t DATASET_SIZE = 5000000; 

void ProcessDataset(std::array<int, DATASET_SIZE>& set, int& sum)
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

int main()
{
	Timer timer; 
	std::minstd_rand randomNumberEngine; 
	std::vector<std::array<int, DATASET_SIZE>> datasets{4}; 
	std::vector<std::thread> workers; 

	for (auto& arr : datasets)
	{
		//Generate random ranges. Just make this long
		std::ranges::generate(arr, randomNumberEngine); 
	}

	struct Value
	{
		int v; //4 byte data type
		char padding[60]; //60 bytes to add up to 64 bytes. 
	};

	Value sum[4] = { 0, 0, 0, 0 };

	timer.StartTimer();

	for (size_t i = 0; i < 4; i++)
	{
		workers.push_back(std::thread{ProcessDataset, std::ref(datasets[i]), std::ref(sum[i].v)});
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

