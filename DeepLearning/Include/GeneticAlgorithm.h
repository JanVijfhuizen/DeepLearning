#pragma once
#include "JLib/Arena.h"
#include "NNet.h"

namespace jv::ai 
{
	struct GeneticAlgorithmRunInfo final
	{
		uint32_t inputSize, outputSize;
		// Amount of instances in a generation.
		uint32_t width = 1000;
		// Amount of generational cycles.
		uint32_t epochs = 1000;
		// Remaining nnets that pass unchanged to the next generation.
		uint32_t survivors = 100;
		// New instances added to each new generation.
		uint32_t arrivals = 100;
		// Stagnate after x epochs without improvements. Will swap to a mode where values are changed in a diminishing way
		// until success if found again.
		uint32_t stagnateAfter = 10;
		// Mutation chances are multiplied by this every unsuccesfull epoch. Resets on success.
		float stagnationMul = .99f;
		// Memory reserved for the algorithm. 
		// Will increase dynamically if there is no space, but will obviously fragment if that happens.
		size_t initMemSize = 33554432;
		float (*ratingFunc)(NNet& nnet, void* userPtr, Arena& arena, Arena& tempArena);
		// Will stop the algorithm if the target score is met.
		float targetScore = -1;
		void* userPtr;
		// Debug progress in command prompt.
		bool debug = true;
	};

	[[nodiscard]] NNet RunGeneticAlgorithm(GeneticAlgorithmRunInfo& info, Arena& arena, Arena& tempArena);
}