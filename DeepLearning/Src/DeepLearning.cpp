#include "pch.h"

#include "BackTrader.h"
#include "JLib/ArrayUtils.h"
#include "JLib/Math.h"
#include "JLib/Queue.h"
#include "NNet.h"
#include <NNetUtils.h>

[[nodiscard]] float GetMAValue(const jv::bt::TimeSeries& stock, const uint32_t offset, void* userPtr)
{
	const float maShort = jv::bt::GetMA(stock.close, offset, 10);
	const float maLong = jv::bt::GetMA(stock.close, offset, 100);

	return maShort / maLong - 1.f;
}

[[nodiscard]] float GetMomentumValue(const jv::bt::TimeSeries& stock, const uint32_t offset, void* userPtr)
{
	float momentum = 0;
	for (uint32_t i = 0; i < 3; ++i)
	{
		const auto ceiling = jv::Max<float>(stock.close[offset + i], stock.open[offset + i]);
		const auto floor = jv::Min<float>(stock.close[offset + i], stock.open[offset + i]);

		float f = stock.high[offset + i] - ceiling;
		f -= floor - stock.low[offset + i];
		momentum /= abs(stock.close[offset + i] - stock.open[offset + i]);
		momentum += f;
	}
	return momentum / 3;
}

[[nodiscard]] float GetTrendValue(const jv::bt::TimeSeries& stock, const uint32_t offset, void* userPtr)
{
	float trend = 0;
	for (uint32_t i = 0; i < 3; ++i)
		trend += stock.close[offset + i] - stock.open[offset + i];
	return trend / 3;
}

void StockAlgorithm(jv::Arena& tempArena, const jv::bt::World& world, const jv::bt::Portfolio& portfolio,
	jv::Vector<jv::bt::Call>& calls, const uint32_t offset, void* userPtr)
{
	jv::bt::Call call{};

	const auto& stock = world.timeSeries[0];
	const float ma = GetMAValue(stock, offset, userPtr);
	const float momentum = GetMomentumValue(stock, offset, userPtr);
	const float trend = GetTrendValue(stock, offset, userPtr);

	auto nnet = reinterpret_cast<jv::ai::NNet*>(userPtr);
	float input[3]{ ma, momentum / 10, trend };
	float output[3];
	Clean(*nnet);
	Propagate(*nnet, input, output);

	uint32_t result = 0;
	result = output[1] > output[0] ? 1 : result;
	result = output[2] > output[1] ? 2 : result;

	if (result == 0)
		return;

	if(result == 1)
	{
		if (portfolio.liquidity - 100 > stock.close[offset])
		{
			call.amount = (portfolio.liquidity - 100) / stock.close[offset];
			call.type = jv::bt::CallType::Buy;
			call.symbolId = 0;
			calls.Add() = call;
		}
	}
	else if(result == 2)
	{
		if (portfolio.liquidity > 10 && portfolio.stocks[0] > 0)
		{
			call.amount = portfolio.stocks[0];
			call.type = jv::bt::CallType::Sell;
			call.symbolId = 0;
			calls.Add() = call;
		}
	}
}

int main()
{
	srand(time(nullptr));

	jv::bt::BackTraderEnvironment bte;

	{
		const char* symbols[4];
		symbols[0] = "AAPL";
		symbols[1] = "AMZN";
		symbols[2] = "TSLA";
		symbols[3] = "EA";
		bte = jv::bt::CreateBTE(symbols, 4, 1e-3);
	}

	jv::ai::NNetCreateInfo nnetCreateInfo{};
	nnetCreateInfo.inputSize = 4;
	nnetCreateInfo.neuronCapacity = 512;
	nnetCreateInfo.weightCapacity = 512;
	nnetCreateInfo.outputSize = 3;
	auto nnet = jv::ai::CreateNNet(nnetCreateInfo, bte.arena);
	auto ioLayers = Init(nnet, jv::ai::InitType::random);
	ConnectIO(nnet, jv::ai::InitType::random);

	auto nnetCpy = jv::ai::CreateNNet(nnetCreateInfo, bte.arena);

	jv::bt::TimeSeries timeSeries = bte.backTrader.world.timeSeries[0];
	//jv::bt::Tracker::Debug(timeSeries.close, 30, true);
	//jv::bt::Tracker::DebugCandles(timeSeries, 0, 400);
	
	jv::bt::TestInfo testInfo{};
	testInfo.bot = StockAlgorithm;
	testInfo.userPtr = &nnetCpy;

	jv::ai::Mutations mutations{};
	mutations.threshold.chance = .2;
	mutations.weight.chance = .2;
	mutations.newNodeChance = .5;
	mutations.newWeightChance = .5;
	
	float highestScore = 0;
	for (size_t i = 0; i < 1000; i++)
	{
		Copy(nnet, nnetCpy);
		Mutate(nnetCpy, mutations);
		const auto ret = bte.backTrader.RunTestEpochs(bte.arena, bte.tempArena, testInfo);

		std::cout << "e" << i << ".";
		if (ret > highestScore)
		{
			highestScore = ret;
			Copy(nnetCpy, nnet);
			std::cout << std::endl << std::endl << highestScore * 100 << "%" << std::endl << std::endl;
		}
	}

	//bte.backTrader.PrintAdvice(bte.arena, bte.tempArena, StockAlgorithm, "jan", true, &nnet);
	return 0;
}
