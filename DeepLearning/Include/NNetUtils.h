#pragma once
#include "NNet.h"

namespace jv::ai 
{
	struct Layer final 
	{
		uint32_t from = UINT32_MAX;
		uint32_t to = UINT32_MAX;
	};

	struct IOLayers
	{
		Layer input, output;
	};

	struct Mutation final
	{
		float weightValueChance = .02;
		float thresholdValueChance = .02;
	};

	enum class InitType 
	{
		flat,
		random
	};

	/* 
	Initialize neural network with default input / output layers.
	Returns the input layer.
	*/
	IOLayers Init(NNet& nnet, InitType initType);
	// Adds a new layer of neurons.
	Layer AddLayer(NNet& nnet, uint32_t length, InitType initType);
	void Connect(NNet& nnet, Layer from, Layer to, InitType initType);
	// Connect the input and output layers.
	void ConnectIO(NNet& nnet, InitType initType);

	void Mutate(NNet& nnet, Mutation mutation);
	void Copy(NNet& org, NNet& dst);
}

