#pragma once

//Settings for now
inline constexpr bool ChunkMeasurementEnabled = false;
inline constexpr size_t WORKER_COUNT = 4;
inline constexpr size_t CHUNK_SIZE = 8000;
inline constexpr size_t CHUNK_COUNT = 100;
inline constexpr size_t SUBSET_SIZE = CHUNK_SIZE / WORKER_COUNT;
inline constexpr size_t LIGHT_ITERATIONS = 100;
inline constexpr size_t HEAVY_ITERATIONS = 1000;
inline constexpr double ProbabilityHeavy = .15;

static_assert(CHUNK_SIZE >= WORKER_COUNT);
static_assert(CHUNK_SIZE% WORKER_COUNT == 0);