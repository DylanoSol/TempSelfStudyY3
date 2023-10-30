#pragma once
#include <random>
#include <array>
#include <ranges>
#include <cmath>
#include <numbers>
#include "Globals.h"

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
			unsigned int digits = unsigned int(std::abs(std::sin(std::cos(intermediate) * std::numbers::pi) * 10000000)) % 100000; //Module slice out some digits.
			intermediate = double(digits) / 10000.;
		}
		return unsigned int(std::exp(intermediate));
	}
};


std::vector<std::array<Task, CHUNK_SIZE>> GenerateDatasetsEvenly()
{
	std::minstd_rand randomNumberEngine;
	std::uniform_real_distribution dist {0., 2. * std::numbers::pi};

	const int everyNth = int(1. / ProbabilityHeavy);
	std::vector<std::array<Task, CHUNK_SIZE>> Chunks(CHUNK_COUNT);

	for (auto& chunk : Chunks)
	{
		//Generate random ranges. Just make this long
		std::ranges::generate(chunk, [&, acc = 0]() mutable {
			bool heavy = false;
			if ((acc += ProbabilityHeavy) >= 1.)
			{
				acc -= 1.;
				heavy = true;
			}
			return Task{ .val = dist(randomNumberEngine), .heavy = heavy };
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
	std::uniform_real_distribution dist {0., 2. * std::numbers::pi};
	std::bernoulli_distribution dist2 {ProbabilityHeavy};
	std::vector<std::array<Task, CHUNK_SIZE>> Chunks(CHUNK_COUNT);

	for (auto& chunk : Chunks)
	{
		//Generate random ranges. Just make this long
		std::ranges::generate(chunk, [&] { return Task{ .val = dist(randomNumberEngine), .heavy = dist2(randomNumberEngine) };  });
	}

	return Chunks;
}