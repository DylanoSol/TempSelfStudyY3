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

//Following along with video tutorial series by ChiliTomatoNoodle
constexpr size_t DATASET_SIZE = 50000000; 

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

	auto datasets = GenerateDatasets(); 

	int grandTotal = 0; 
	std::vector<std::jthread> workers;
	constexpr const auto subsetSize = DATASET_SIZE;
	for (size_t i = 0; i < DATASET_SIZE; i += subsetSize)
	{
		for (size_t j = 0; j < 4; j++)
		{
			workers.push_back(std::jthread{ProcessDataset, std::span{&datasets[j][i], subsetSize}, std::ref(sum[j].v)});
		}
		workers.clear(); //Destroy the threads. 
		grandTotal = sum[0].v + sum[1].v + sum[2].v + sum[3].v; 
	}

	float timeElapsed = timer.GetTime();
	printf("%f milliseconds \n", timeElapsed);
	printf("%d sum \n", grandTotal);

	return 0; 
}

int main(int argc, char** argv)
{
	
	return SmallOperation(); 
	//return BigOperation(); 
}
