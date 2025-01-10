#include "pch.h"
#include "GeneticAlgorithm.h"
#include <JLib/LinearSort.h>
#include <NNetUtils.h>

namespace jv::ai
{
	void* Alloc(const uint32_t size)
	{
		return malloc(size);
	}
	void Free(void* ptr)
	{
		return free(ptr);
	}

	bool Comparer(float& a, float& b) 
	{
		return a > b;
	}

	NNet RunGeneticAlgorithm(GeneticAlgorithmRunInfo& info, Arena& arena, Arena& tempArena)
	{
		const auto tempScope = tempArena.CreateScope();
		float* ratings = tempArena.New<float>(info.width);
		uint32_t* indices = tempArena.New<uint32_t>(info.width);
		NNet bestNNet{};
		float bestNNetRating = -1;

		Arena arenas[2];

		{
			// Allocate memory from temp arena with predetermined minimal size.
			ArenaCreateInfo createInfo{};
			createInfo.alloc = Alloc;
			createInfo.free = Free;
			createInfo.memory = tempArena.Alloc(info.initMemSize / 2);
			createInfo.memorySize = info.initMemSize / 2;
			arenas[0] = Arena::Create(createInfo);
			createInfo.memory = tempArena.Alloc(info.initMemSize / 2);
			arenas[1] = Arena::Create(createInfo);
		}

		jv::ai::Mutations mutations{};
		mutations.threshold.chance = .2;
		mutations.weight.chance = .2;
		mutations.newNodeChance = .5;
		mutations.newWeightChance = .5;

		NNet* generations[2];
		for (uint32_t i = 0; i < 2; i++)
			generations[i] = tempArena.New<NNet>(info.width);

		uint32_t mutationId = 0;

		// Set up first generation of random instances.
		for (uint32_t i = 0; i < info.width; i++)
		{
			NNet& nnet = generations[0][i];
			nnet = CreateNNet(info.nnetCreateInfo, arenas[0]);
			Init(nnet, InitType::random, mutationId);
			ConnectIO(nnet, jv::ai::InitType::random, mutationId);
		}

		// Scope used to store best nnet in.
		auto retScope = arena.CreateScope();

		for (uint32_t i = 0; i < info.epochs; i++)
		{
			// Old/New generation index.
			uint32_t oInd = i % 2;
			uint32_t nInd = 1 - oInd;

			// Rate every instance of the generation.
			for (uint32_t j = 0; j < info.width; j++)
			{
				NNet& nnet = generations[oInd][j];
				ratings[j] = info.ratingFunc(nnet, info.userPtr, arena, tempArena);
			}

			CreateSortableIndices(indices, info.width);
			ExtLinearSort(ratings, indices, info.width, Comparer);

			const uint32_t hw = info.width / 2;
			// Copy best performing nnets to new generation.
			for (uint32_t j = 0; j < info.survivors; j++)
				Copy(generations[oInd][indices[j]], generations[nInd][j], &arenas[nInd]);

			// Delete old generation by wiping the entire arena.
			arenas[oInd].Clear();

			const auto nGen = generations[nInd];
			uint32_t breededCount = info.width - info.survivors - info.arrivals;

			if (Comparer( ratings[indices[0]], bestNNetRating))
			{
				bestNNetRating = ratings[indices[0]];
				arena.DestroyScope(retScope);
				Copy(generations[nInd][0], bestNNet, &arena);
				retScope = arena.CreateScope();
				std::cout << std::endl << std::endl << bestNNetRating * 100 << "%" << std::endl << std::endl;
			}

			// Breed new generation.
			for (uint32_t j = 0; j < breededCount; j++)
			{
				auto& a = nGen[rand() % info.survivors];
				auto& b = nGen[rand() % info.survivors];
				auto& c = nGen[info.survivors + j] = Breed(a, b, arenas[nInd], tempArena);
				Mutate(c, mutations, mutationId);
			}

			// Add new random arrivals.
			for (uint32_t j = 0; j < info.arrivals; j++)
			{
				auto& nnet = generations[nInd][info.width - j - 1];
				nnet = CreateNNet(info.nnetCreateInfo, arenas[nInd]);
				Init(nnet, InitType::random, mutationId);
				ConnectIO(nnet, jv::ai::InitType::random, mutationId);
			}

			std::cout << "e" << i << ".";
		}

		Arena::Destroy(arenas[1]);
		Arena::Destroy(arenas[0]);
		tempArena.DestroyScope(tempScope);
		return bestNNet;
	}
}