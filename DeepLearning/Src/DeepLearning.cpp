#include "pch.h"

#include "BackTrader.h"
#include "JLib/ArrayUtils.h"
#include "JLib/Math.h"
#include "JLib/Queue.h"

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

	if(ma + momentum + trend > 0)
	{
		if (portfolio.liquidity - 100 > stock.close[offset])
		{
			call.amount = (portfolio.liquidity - 100) / stock.close[offset];
			call.type = jv::bt::CallType::Buy;
			call.symbolId = 0;
			calls.Add() = call;
		}
	}
	else
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

	jv::bt::TimeSeries timeSeries = bte.backTrader.world.timeSeries[0];
	//jv::bt::Tracker::Debug(timeSeries.close, 30, true);
	//jv::bt::Tracker::DebugCandles(timeSeries, 0, 400);
	
	jv::bt::TestInfo testInfo{};
	testInfo.bot = StockAlgorithm;
	
	const auto ret = bte.backTrader.RunTestEpochs(bte.arena, bte.tempArena, testInfo);
	std::cout << ret * 100 << "%" << std::endl;

	bte.backTrader.PrintAdvice(bte.arena, bte.tempArena, StockAlgorithm, "jan", true);
	return 0;
}
