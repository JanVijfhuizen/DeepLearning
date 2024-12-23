#include "pch.h"
#include "NNetUtils.h"
#include "Jlib/Math.h"

namespace jv::ai
{
	IOLayers Init(NNet& nnet, const InitType initType)
	{
		auto inputLayer = AddLayer(nnet, nnet.createInfo.inputSize, initType);
		auto outputLayer = AddLayer(nnet, nnet.createInfo.outputSize, initType);
		return { inputLayer, outputLayer };
	}

	Layer AddLayer(NNet& nnet, const uint32_t length, InitType initType)
	{
		for (uint32_t i = 0; i < length; i++)
		{
			bool valid = true;
			switch (initType)
			{
			case InitType::flat:
				valid = AddNeuron(nnet, 1, 0);
				break;
			case InitType::random:
				valid = AddNeuron(nnet, jv::RandF(0, 1), jv::RandF(0, 1));
				break;
			default:
				break;
			}
			assert(valid);
		}
			
		return { nnet.neuronCount - length, nnet.neuronCount };
	}

	void Connect(NNet& nnet, Layer from, Layer to, InitType initType)
	{
		const uint32_t inSize = from.to - from.from;
		const uint32_t outSize = to.to - to.from;

		for (uint32_t i = 0; i < inSize; i++)
			for (uint32_t j = 0; j < outSize; j++)
			{
				bool valid = true;
				switch (initType)
				{
				case InitType::flat:
					valid = AddWeight(nnet, from.from + i, to.from + j, 1);
					break;
				case InitType::random:
					valid = AddWeight(nnet, from.from + i, to.from + j, jv::RandF(-1, 1));
					break;
				default:
					break;
				}
				assert(valid);
			}	
	}

	void ConnectIO(NNet& nnet, const InitType initType)
	{
		const uint32_t inSize = nnet.createInfo.inputSize;
		const uint32_t outSize = nnet.createInfo.outputSize;
		Connect(nnet, { 0, inSize }, { inSize, inSize + outSize }, initType);
	}
	float GetCompability(NNet& a, NNet& b)
	{
		uint32_t errorCount = 0;
		uint32_t aC = 0;
		uint32_t bC = 0;
		uint32_t wC = 0;

		while (aC < a.weightCount && bC < b.weightCount)
		{
			auto& aW = a.weights[aC];
			auto& bW = b.weights[bC];
			
			const bool eq = aW.innovationId == bW.innovationId;
			if (!eq)
				++errorCount;
			aC += aW.innovationId < bW.innovationId || eq;
			bC += bW.innovationId < aW.innovationId || eq;
			++wC;
		}
		errorCount += a.weightCount - aC + b.weightCount - bC;
		return 1.f - static_cast<float>(errorCount) / static_cast<float>(wC);
	}
	NNet Breed(NNet& a, NNet& b, Arena& arena, Arena& tempArena)
	{
		const auto tempScope = tempArena.CreateScope();
		NNetCreateInfo createInfo = a.createInfo;
		createInfo.neuronCapacity += b.createInfo.neuronCapacity;
		createInfo.weightCapacity += b.createInfo.weightCapacity;
		auto tempNNet = CreateNNet(createInfo, tempArena);

		uint32_t aC = 0;
		uint32_t bC = 0;
		uint32_t nC = 0;

		// Ordered double insert.
		while (aC < a.neuronCount && bC < b.neuronCount)
		{
			auto& aN = a.neurons[aC];
			auto& bN = b.neurons[bC];

			const bool eq = aN.innovationId == bN.innovationId;

			// Either add neuron from a, b or random.
			if (aN.innovationId < bN.innovationId)
				tempNNet.neurons[tempNNet.neuronCount++] = aN;
			if (aN.innovationId == bN.innovationId)
				tempNNet.neurons[tempNNet.neuronCount++] = rand() % 2 ? aN : bN;
			if (aN.innovationId > bN.innovationId)
				tempNNet.neurons[tempNNet.neuronCount++] = bN;

			aC += aN.innovationId < bN.innovationId || eq;
			bC += bN.innovationId < aN.innovationId || eq;
			++nC;
		}
		while (aC < a.neuronCount)
			tempNNet.neurons[tempNNet.neuronCount++] = a.neurons[aC++];
		while (bC < b.neuronCount)
			tempNNet.neurons[tempNNet.neuronCount++] = b.neurons[bC++];

		tempArena.DestroyScope(tempScope);
		return {};
	}

	void Mutate(NNet& nnet, const Mutations mutations)
	{
		auto& weightMut = mutations.weight;
		if (weightMut.chance > 0)
		{
			for (size_t i = 0; i < nnet.weightCount; i++)
			{
				if (RandF(0, 1) > weightMut.chance)
					continue;

				auto& weight = nnet.weights[i];
				// 1 = new value, 2 = percent wise, 3 = linear addition/subtraction.
				uint32_t type = rand() % 3;
				weight.value = type != 0 ? weight.value : RandF(-1, 1);
				weight.value = type != 1 ? weight.value : weight.value * 
					RandF(1.f - weightMut.pctAlpha, 1.f + weightMut.pctAlpha);
				weight.value = type != 2 ? weight.value : weight.value + RandF(-1, 1) * weightMut.linAlpha;
			}
		}
		auto& thresholdMut = mutations.threshold;
		if (thresholdMut.chance > 0)
		{
			for (size_t i = 0; i < nnet.neuronCount; i++)
			{
				if (RandF(0, 1) > thresholdMut.chance)
					continue;

				auto& neuron = nnet.neurons[i];
				uint32_t type = rand() % 3;
				neuron.threshold = type != 0 ? neuron.threshold : RandF(0, 1);
				neuron.threshold = type != 1 ? neuron.threshold : neuron.threshold *
					RandF(1.f - thresholdMut.pctAlpha, 1.f + thresholdMut.pctAlpha);
				neuron.threshold = type != 2 ? neuron.threshold : neuron.threshold + RandF(-1, 1) * thresholdMut.linAlpha;
				neuron.threshold = Max<float>(neuron.threshold, .1);
			}
		}
		auto& decayMut = mutations.decay;
		if (decayMut.chance > 0)
		{
			for (size_t i = 0; i < nnet.neuronCount; i++)
			{
				if (RandF(0, 1) > decayMut.chance)
					continue;

				auto& neuron = nnet.neurons[i];
				uint32_t type = rand() % 3;
				neuron.decay = type != 0 ? neuron.decay : RandF(0, 1);
				neuron.decay = type != 1 ? neuron.decay : neuron.decay *
					RandF(1.f - decayMut.pctAlpha, 1.f + decayMut.pctAlpha);
				neuron.decay = type != 2 ? neuron.decay : neuron.decay + RandF(-1, 1) * decayMut.linAlpha;
				neuron.decay = Clamp<float>(neuron.decay, 0, .9);
			}
		}
		if (RandF(0, 1) < mutations.newNodeChance && nnet.weightCount < nnet.createInfo.weightCapacity)
		{
			bool valid = AddNeuron(nnet, RandF(0, 1), RandF(0, 1));
			if (valid)
			{
				const uint32_t weightId = rand() % nnet.weightCount;
				auto& weight = nnet.weights[weightId];
				weight.enabled = false;
				AddWeight(nnet, weight.from, nnet.neuronCount - 1, weight.value);
				AddWeight(nnet, nnet.neuronCount - 1, weight.to, 1);
			}
		}
		if (RandF(0, 1) < mutations.newWeightChance)
			// Minimize the impact this weight has on the network itself, making it mostly a topology based evolution.
			AddWeight(nnet, rand() % nnet.neuronCount, nnet.createInfo.inputSize + rand() % 
				(nnet.neuronCount - nnet.createInfo.inputSize), RandF(-.1, .1));
	}

	void Copy(NNet& org, NNet& dst)
	{
		dst.neuronCount = org.neuronCount;
		dst.weightCount = org.weightCount;
		memcpy(dst.neurons, org.neurons, sizeof(Neuron) * org.neuronCount);
		memcpy(dst.weights, org.weights, sizeof(Weight) * org.weightCount);
	}
}