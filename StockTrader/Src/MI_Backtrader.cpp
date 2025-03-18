#include "pch.h"
#include "MenuItems/MI_Backtrader.h"
#include "MenuItems/MI_Symbols.h"
#include <JLib/ArrayUtils.h>
#include <Utils/UT_Colors.h>
#include <Ext/ImGuiLoadBar.h>
#include <Utils/UT_Time.h>

namespace jv::bt
{
	constexpr int32_t MAX_ZOOM = 30;
	
	enum BTMenuIndex
	{
		btmiPortfolio,
		btmiAlgorithms,
		btmiRunInfo
	};

	void MI_Backtrader::Load(STBT& stbt)
	{
		const auto tempScope = stbt.tempArena.CreateScope();

		names = MI_Symbols::GetSymbolNames(stbt);
		enabled = MI_Symbols::GetEnabled(stbt, names, enabled);

		uint32_t c = 0;
		for (uint32_t i = 0; i < names.length; i++)
			c += enabled[i];

		buffers = CreateArray<char*>(stbt.arena, c + 1);
		for (uint32_t i = 0; i < buffers.length; i++)
			buffers[i] = stbt.arena.New<char>(16);
		timeSeries = CreateArray<TimeSeries>(stbt.arena, c);

		auto namesCharPtrs = CreateArray<const char*>(stbt.tempArena, names.length);

		uint32_t index = 0;
		for (size_t i = 0; i < enabled.length; i++)
		{
			if (!enabled[i])
				continue;
			uint32_t _;
			namesCharPtrs[index] = names[i].c_str();
			timeSeries[index++] = MI_Symbols::LoadSymbol(stbt, i, names, _);			
		}

		portfolio = Portfolio::Create(stbt.arena, namesCharPtrs.ptr, names.length);
		for (uint32_t i = 0; i < c; i++)
			portfolio.stocks[i].symbol = namesCharPtrs[i];

		subIndex = 0;
		symbolIndex = -1;
		if (enabled.length > 0)
			symbolIndex = 0;
		normalizeGraph = false;

		algoIndex = -1;
		stepwise = false;
		log = false;
		pauseOnFinish = false;
		running = false;

		trades = stbt.arena.New<STBTTrade>(timeSeries.length);

		uint32_t n = 1;
		snprintf(runCountBuffer, sizeof(runCountBuffer), "%i", n);
		snprintf(batchBuffer, sizeof(batchBuffer), "%i", n);
		snprintf(buffBuffer, sizeof(buffBuffer), "%i", n);
		n = MAX_ZOOM;
		snprintf(zoomBuffer, sizeof(zoomBuffer), "%i", n);
		float f = 1e-3f;
		snprintf(feeBuffer, sizeof(feeBuffer), "%f", f);
		f = 10000;
		snprintf(buffers[0], sizeof(buffers[0]), "%f", f);

		stbt.tempArena.DestroyScope(tempScope);
		subScope = stbt.arena.CreateScope();
	}

	bool MI_Backtrader::DrawMainMenu(STBT& stbt, uint32_t& index)
	{
		if (running)
			return false;

		const char* buttons[]
		{
			"Environment",
			"Algorithms",
			"Run Info"
		};

		for (uint32_t i = 0; i < 3; i++)
		{
			const bool selected = subIndex == i;
			if (selected)
				ImGui::PushStyleColor(ImGuiCol_Text, { 0, 1, 0, 1 });
			if (ImGui::Button(buttons[i]))
				subIndex = i;
			if (selected)
				ImGui::PopStyleColor();
		}

		if (ImGui::Button("Back"))
			index = 0;

		return false;
	}

	bool MI_Backtrader::DrawSubMenu(STBT& stbt, uint32_t& index)
	{
		if (running)
		{
			DrawLog(stbt);
			return false;
		}
		
		if (subIndex == btmiPortfolio)
		{
			ImGui::Text("Portfolio");

			uint32_t index = 0;
			ImGui::InputText("Cash", buffers[0], 9, ImGuiInputTextFlags_CharsScientific);

			for (uint32_t i = 0; i < names.length; i++)
			{
				if (!enabled[i])
					continue;
				++index;

				ImGui::PushID(i);
				if (ImGui::InputText("##", buffers[index], 9, ImGuiInputTextFlags_CharsDecimal))
				{
					int32_t n = std::atoi(buffers[index]);
					n = Max(n, 0);
					snprintf(buffers[index], sizeof(buffers[index]), "%i", n);
				}
				ImGui::PopID();
				ImGui::SameLine();

				const bool selected = symbolIndex == i;
				if (selected)
					ImGui::PushStyleColor(ImGuiCol_Text, { 0, 1, 0, 1 });

				const auto symbol = names[i].c_str();
				if (ImGui::Button(symbol))
					symbolIndex = i;

				if (selected)
					ImGui::PopStyleColor();
			}
		}
		if (subIndex == btmiAlgorithms)
		{
			for (uint32_t i = 0; i < stbt.bots.length; i++)
			{
				auto& bot = stbt.bots[i];
				const bool selected = i == algoIndex;

				if (selected)
					ImGui::PushStyleColor(ImGuiCol_Text, { 0, 1, 0, 1 });
				if (ImGui::Button(bot.name))
					algoIndex = i;
				if (selected)
					ImGui::PopStyleColor();

				if (selected)
				{
					ImGui::Text(bot.author);
					ImGui::Text(bot.description);
				}
			}
		}
		if (subIndex == btmiRunInfo)
		{
			if (ImGui::InputText("Runs", runCountBuffer, 4, ImGuiInputTextFlags_CharsDecimal))
			{
				int32_t n = std::atoi(runCountBuffer);
				n = Max(n, 1);
				snprintf(runCountBuffer, sizeof(runCountBuffer), "%i", n);
			}

			if (ImGui::InputText("Buffer", buffBuffer, 5, ImGuiInputTextFlags_CharsDecimal))
			{
				int32_t n = std::atoi(buffBuffer);
				n = Max(n, 1);
				snprintf(buffBuffer, sizeof(buffBuffer), "%i", n);
			}

			if (ImGui::InputText("Fee", feeBuffer, 8, ImGuiInputTextFlags_CharsDecimal))
			{
				float n = std::atof(feeBuffer);
				n = Max(n, 0.f);
				snprintf(feeBuffer, sizeof(feeBuffer), "%f", n);
			}

			if (ImGui::InputText("Zoom", zoomBuffer, 3, ImGuiInputTextFlags_CharsDecimal))
			{
				int32_t n = std::atoi(zoomBuffer);
				n = Clamp(n, 2, MAX_ZOOM);
				snprintf(zoomBuffer, sizeof(zoomBuffer), "%i", n);
			}

			if (ImGui::InputText("Batches", batchBuffer, 5, ImGuiInputTextFlags_CharsDecimal))
			{
				int32_t n = std::atoi(batchBuffer);
				n = Max(n, 1);
				snprintf(batchBuffer, sizeof(batchBuffer), "%i", n);
			}
			
			ImGui::Checkbox("Stepwise", &stepwise);
			ImGui::Checkbox("Pause On Finish", &pauseOnFinish);
			ImGui::Checkbox("Randomize Date", &randomizeDate);
			ImGui::Checkbox("Log", &log);

			if (randomizeDate)
			{
				if (ImGui::InputText("Length", lengthBuffer, 5, ImGuiInputTextFlags_CharsDecimal))
				{
					int32_t n = std::atoi(lengthBuffer);
					n = Max(n, 0);
					snprintf(lengthBuffer, sizeof(lengthBuffer), "%i", n);
				}
			}

			if (ImGui::Button("Run"))
			{
				bool valid = true;
				if (algoIndex == -1)
				{
					valid = false;
					stbt.output.Add() = "ERROR: No algorithm selected!";
				}

				if (symbolIndex == -1)
				{
					valid = false;
					stbt.output.Add() = "ERROR: No symbols available!";
				}

				if (valid)
				{
					// check buffer w/ min date
					const int32_t buffer = std::atoi(buffBuffer);
					const int32_t length = std::atoi(lengthBuffer);

					const auto tFrom = mktime(&stbt.from);
					const auto tTo = mktime(&stbt.to);

					const auto diff = difftime(tTo, tFrom);
					const uint32_t daysDiff = diff / 60 / 60 / 24;

					if (daysDiff < buffer + 1)
					{
						valid = false;
						stbt.output.Add() = "ERROR: Buffer range is out of scope!";
					}

					if (randomizeDate && daysDiff < buffer + length)
					{
						valid = false;
						stbt.output.Add() = "ERROR: Length is out of scope!";
					}

					if (valid)
					{
						running = true;
						runIndex = -1;
						batchId = 0;
						timeElapsed = 0;
					}
				}
			}
		}
		return false;
	}

	bool MI_Backtrader::DrawFree(STBT& stbt, uint32_t& index)
	{
		if(!running)
			MI_Symbols::TryRenderSymbol(stbt, timeSeries, names, enabled, symbolIndex, normalizeGraph);
		BackTest(stbt, true);
		return false;
	}

	const char* MI_Backtrader::GetMenuTitle()
	{
		return "Backtrader";
	}

	const char* MI_Backtrader::GetSubMenuTitle()
	{
		return "Details";
	}

	const char* MI_Backtrader::GetDescription()
	{
		return "Test trade algorithms \nin paper trading.";
	}

	void MI_Backtrader::Unload(STBT& stbt)
	{
	}

	void MI_Backtrader::BackTest(STBT& stbt, bool render)
	{
		if (running)
		{
			const auto tFrom = mktime(&stbt.from);
			const auto tTo = mktime(&stbt.to);

			const auto diff = difftime(tTo, tFrom);
			const uint32_t daysDiff = diff / 60 / 60 / 24;

			const int32_t length = std::atoi(runCountBuffer);
			uint32_t runLength = randomizeDate ? std::atoi(lengthBuffer) : daysDiff;
			uint32_t buffer = std::atoi(buffBuffer);
			if (buffer >= runLength)
			{
				stbt.output.Add() = "ERROR: Buffer is larger than run length. \n Aborting run.";
				running = false;
			}
			runLength -= buffer;

			auto& bot = stbt.bots[algoIndex];

			bool canFinish = !pauseOnFinish;
			bool canEnd = false;

			if(render)
			{
				MI_Symbols::DrawBottomRightWindow("Current Run");

				const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
				const ImU32 bg = ImGui::GetColorU32(ImGuiCol_Button);

				std::string runText = "Epoch " + std::to_string(runIndex + 1);
				runText += "/";
				runText += std::to_string(length);

				if (runDayIndex != -1)
				{
					runText += " Day " + std::to_string(runDayIndex);
					runText += "/";
					runText += std::to_string(runLength);
				}
				if (runIndex == -1)
					runText = "Preprocessing data.";
				ImGui::Text(runText.c_str());

				if (!stepwise)
				{
					std::string elapsed = "Elapsed/Remaining: " + ConvertSecondsToHHMMSS(timeElapsed / 1e6) + "/";
					
					float e = timeElapsed;
					e /= runDayIndex + runIndex * runLength;
					const float avrFrame = e;
					const float totalDuration = avrFrame * (length * runLength);
					e *= (length - runIndex - 1) * runLength + (runLength - runDayIndex);
					elapsed += ConvertSecondsToHHMMSS(e / 1e6);
					ImGui::Text(elapsed.c_str());
					//ImGui::Text(ConvertSecondsToHHMMSS(totalDuration / 1e6).c_str());
				}
				
				if (runDayIndex >= runLength && pauseOnFinish)
				{
					if (ImGui::Button("Continue"))
						canFinish = true;
					ImGui::SameLine();
					if (runIndex < length - 1 && ImGui::Button("Break"))
						canEnd = true;
				}
				else if (stepwise && stepCompleted)
				{
					if (ImGui::Button("Continue"))
						stepCompleted = false;
					ImGui::SameLine();
					if (ImGui::Button("Break"))
					{
						runDayIndex = runLength;
						stepCompleted = false;
						canFinish = true;
						canEnd = true;
					}
					ImGui::SameLine();
					ImGui::PushItemWidth(40);
					if (ImGui::InputText("Zoom", zoomBuffer, 3, ImGuiInputTextFlags_CharsDecimal))
					{
						int32_t n = std::atoi(zoomBuffer);
						n = Clamp(n, 2, MAX_ZOOM);
						snprintf(zoomBuffer, sizeof(zoomBuffer), "%i", n);
					}

					if (ImGui::Button("Stepwise off"))
						stepwise = false;
				}
				else 
				{
					if (ImGui::Button("Stepwise on"))
						stepwise = true;
				}

				ImGui::End();

				MI_Symbols::DrawTopRightWindow("Stocks", true, true);

				std::string liquidity = "Liquidity: ";
				uint32_t ILiq = round(portfolio.liquidity);
				liquidity += std::to_string(ILiq);
				ImGui::Text(liquidity.c_str());

				const uint32_t dayOffsetIndex = runOffset - runDayIndex;

				std::string portValue = "Port Value: ";
				float v = portfolio.liquidity;
				for (uint32_t i = 0; i < timeSeries.length; i++)
				{
					const auto& stock = portfolio.stocks[i];
					const float val = stock.count * timeSeries[i].close[dayOffsetIndex];
					v += val;
					
					std::string t = stock.symbol;
					t += ": ";
					t += std::to_string(stock.count);
					t += ", ";
					t += std::to_string(int(round(val)));
					t += " ";

					const int32_t change = trades[i].change;
					if (change != 0)
					{
						ImVec4 col = change > 0 ? ImVec4{ 0, 1, 0, 1 } : ImVec4{ 1, 0, 0, 1 };
						ImGui::PushStyleColor(ImGuiCol_Text, col);

						if (change > 0)
							t += "+";
						t += std::to_string(change);
					}
						
					ImGui::Text(t.c_str());

					if(change != 0)
						ImGui::PopStyleColor();
				}
				uint32_t iV = round(v);

				portValue += std::to_string(iV);
				ImGui::Text(portValue.c_str());

				ImGui::End();
			}

			if (runIndex == -1)
			{
				runIndex = 0;
				runDayIndex = -1;
			}
			else if (runIndex >= length)
			{
				running = false;
			}
			else
			{
				// If this is a new run, set everything up.
				if (runDayIndex == -1)
				{
					// If random, decide on day.
					const int32_t buffer = std::atoi(buffBuffer);
					const auto tCurrent = GetTime(0);
					const auto cdiff = difftime(tCurrent, tFrom);
					const uint32_t cdaysDiff = cdiff / 60 / 60 / 24;
					const uint32_t maxDiff = daysDiff - buffer - runLength;

					if (randomizeDate)
					{
						const uint32_t randOffset = rand() % maxDiff;
						runOffset = cdaysDiff - randOffset;
					}
					else
						runOffset = cdaysDiff - buffer;

					// Fill portfolio.
					portfolio.liquidity = std::atof(buffers[0]);
					for (uint32_t i = 0; i < timeSeries.length; i++)
						portfolio.stocks[i].count = static_cast<uint32_t>(*buffers[i + 1]);

					stbtScope = STBTScope::Create(&portfolio, timeSeries);
					for (uint32_t i = 0; i < timeSeries.length; i++)
						trades[i].change = 0;

					if (bot.init)
						bot.init(stbtScope, bot.userPtr);
					runDayIndex = 0;

					runScope = stbt.arena.CreateScope();
					runLog = Log::Create(stbt.arena, stbtScope, runOffset - runLength, runOffset);
					stepCompleted = false;
					tpStart = std::chrono::steady_clock::now();

					portPoints = CreateArray<jv::gr::GraphPoint>(stbt.tempArena, runLength);
					avrPoints = CreateArray<jv::gr::GraphPoint>(stbt.tempArena, runLength);
					pctPoints = CreateArray<jv::gr::GraphPoint>(stbt.tempArena, runLength);
				}
				// If this run is completed, either start a new run or quit.
				if (runDayIndex == runLength)
				{
					if (canFinish || canEnd)
					{
						if (bot.cleanup)
							bot.cleanup(stbtScope, bot.userPtr);
						runDayIndex = -1;
						runIndex++;

						stbt.arena.DestroyScope(runScope);

						if (canEnd)
							runIndex = length;
					}
				}
				else if(!stepwise || !stepCompleted)
				{
					auto tpEnd = std::chrono::steady_clock::now();
					auto diff = std::chrono::duration_cast<std::chrono::microseconds>(tpEnd - tpStart).count();
					timeElapsed += diff;
					tpStart = tpEnd;

					const uint32_t dayOffsetIndex = runOffset - runDayIndex;
					const float fee = std::atof(feeBuffer);
					const auto& stocks = portfolio.stocks;

					// Execute trades.
					for (uint32_t i = 0; i < timeSeries.length; i++)
					{
						auto& trade = trades[i];
						auto& stock = portfolio.stocks[i];
						float change = trade.change * timeSeries[i].open[dayOffsetIndex];
						const float feeMod = (1.f + fee * (change > 0 ? 1 : -1));
						change *= feeMod;

						const bool enoughInStock = trade.change > 0 ? true : -trade.change <= stock.count;

						if (change < portfolio.liquidity && enoughInStock)
						{
							stock.count += trade.change;
							portfolio.liquidity -= change;
						}

						trade.change = 0;
					}

					float portfolioValue = 0;
					for (uint32_t i = 0; i < timeSeries.length; i++)
					{
						auto& num = runLog.numsInPort[i][runDayIndex] = stocks[i].count;
						portfolioValue += timeSeries[i].close[dayOffsetIndex] * num;
					}

					runLog.portValues[runDayIndex] = portfolioValue;
					runLog.liquidities[runDayIndex] = portfolio.liquidity;

					float close = 0;
					float closeStart = 0;

					for (uint32_t j = 0; j < timeSeries.length; j++)
					{
						const auto& series = timeSeries[j];
						close += series.close[runOffset - runDayIndex];
						closeStart += series.close[runOffset];
					}

					const float pct = close / closeStart;
					const float avr = (portfolioValue + portfolio.liquidity) / 
						(runLog.portValues[0] + runLog.liquidities[0]) / pct;

					runLog.marktPct[runDayIndex] = pct;
					runLog.marktAvr[runDayIndex] = avr;

					bot.update(stbtScope, trades, dayOffsetIndex, bot.userPtr);
					runDayIndex++;
					stepCompleted = true;
				}
			}

			if (render)
			{
				// Draw the graphs. Only possible if there are at least 2 graph points.
				if (runDayIndex > 0 && runDayIndex != -1)
				{
					const uint32_t dayOffsetIndex = runOffset - runDayIndex;
					const uint32_t l = runDayIndex >= runLength ? runLength - 1 : runDayIndex;
					const auto tScope = stbt.tempArena.CreateScope();

					const uint32_t i = l - 1;
					const auto v = runLog.portValues[i] + runLog.liquidities[i];
					const float avr = runLog.marktAvr[i];
					const float pct = runLog.marktPct[i];

					portPoints[i].open = v;
					portPoints[i].close = v;
					portPoints[i].high = v;
					portPoints[i].low = v;

					avrPoints[i].open = avr;
					avrPoints[i].close = avr;
					avrPoints[i].high = avr;
					avrPoints[i].low = avr;

					pctPoints[i].open = pct;
					pctPoints[i].close = pct;
					pctPoints[i].high = pct;
					pctPoints[i].low = pct;

					auto colors = LoadRandColors(stbt.tempArena, 5);
					const float ratio = stbt.renderer.GetAspectRatio();

					glm::vec2 grPos = { 0, 0 };
					grPos.x += .36f;
					grPos.y += .02f;

					jv::gr::DrawGraphInfo drawInfo{};
					drawInfo.aspectRatio = ratio;
					drawInfo.position = grPos;
					drawInfo.scale = glm::vec2(.9);
					drawInfo.points = portPoints.ptr;
					drawInfo.length = l;
					drawInfo.color = colors[0];
					drawInfo.title = "portfolio value";

					stbt.renderer.SetLineWidth(2);
					stbt.renderer.DrawGraph(drawInfo);
					stbt.renderer.SetLineWidth(1);

					const float top = .8;
					const float bot = -.265;
					const float center = (top + bot) / 2;

					drawInfo.position = { .85f, center };
					drawInfo.scale = glm::vec2(1) / 3.f;
					drawInfo.points = pctPoints.ptr;
					drawInfo.color = colors[1];
					drawInfo.title = "mark";
					stbt.renderer.DrawGraph(drawInfo);

					drawInfo.position.y = bot;
					drawInfo.points = avrPoints.ptr;
					drawInfo.color = colors[2];
					drawInfo.title = "rel";
					stbt.renderer.DrawGraph(drawInfo);

					const uint32_t zoom = std::stoi(zoomBuffer);

					if (l >= zoom && zoom > 0 && zoom <= Min((uint32_t)MAX_ZOOM, runLength))
					{
						drawInfo.noBackground = false;
						std::string zoomPort = "port" + std::to_string(zoom);

						drawInfo.position.y = top;
						drawInfo.points = &portPoints.ptr[l - zoom];
						drawInfo.length = zoom;
						drawInfo.color = colors[3];
						drawInfo.title = zoomPort.c_str();
						stbt.renderer.DrawGraph(drawInfo);

						std::string zoomMarket = "mark" + std::to_string(zoom);
						drawInfo.position.x -= .3f;
						drawInfo.position.y = .8f;
						drawInfo.points = &avrPoints.ptr[l - zoom];
						drawInfo.color = colors[4];
						drawInfo.title = zoomMarket.c_str();
						stbt.renderer.DrawGraph(drawInfo);
					}

					stbt.tempArena.DestroyScope(tScope);
				}
			}

			const int32_t batchLength = stepwise ? 1 : std::atoi(batchBuffer);
			if (++batchId < batchLength && running)
				BackTest(stbt, false);
			else
				batchId = 0;
		}
	}
	void MI_Backtrader::DrawLog(STBT& stbt)
	{
		if (runDayIndex == -1)
			return;

		const uint32_t drawCap = 50;
		const uint32_t length = Min(runDayIndex, drawCap);
		const uint32_t start = drawCap > runDayIndex ? 0 : runDayIndex - drawCap;

		for (uint32_t i = 0; i < length; i++)
		{	
			std::string dayText = "---DAY ";
			dayText += std::to_string(runDayIndex - length + i + 1);
			dayText += "---";
			ImGui::Text(dayText.c_str());

			const float col = .6f;
			ImGui::PushStyleColor(ImGuiCol_Text, { col, col, col, 1 });

			const float portV = runLog.portValues[start + i];
			const float liqV = runLog.liquidities[start + i];

			std::string totalText = "Total Value: ";
			totalText += std::to_string((int)(portV + liqV));
			ImGui::Text(totalText.c_str());

			std::string portText = "Port Value: ";
			portText += std::to_string((int)portV);
			ImGui::Text(portText.c_str());

			std::string liquidText = "Liquidity: ";
			liquidText += std::to_string((int)liqV);
			ImGui::Text(liquidText.c_str());

			const int32_t pId = start + i - 1;
			if (pId >= 0)
			{
				const float portVp = runLog.portValues[pId];
				const float liqVp = runLog.liquidities[pId];

				const auto change = (int)((portV + liqV) - (portVp + liqVp));
				if (change != 0)
				{
					ImVec4 tradeCol = change > 0 ? ImVec4{ 0, 1, 0, 1 } : ImVec4{ 1, 0, 0, 1 };
					ImGui::PushStyleColor(ImGuiCol_Text, tradeCol);
					std::string changeText = "Change: ";
					changeText += std::to_string(change);
					ImGui::Text(changeText.c_str());
					ImGui::PopStyleColor();
				}
			}

			// Print trades.
			if (i > 0)
			{
				for (uint32_t j = 0; j < timeSeries.length; j++)
				{
					const auto& curLog = runLog.numsInPort[j];
					const int diff = curLog[runDayIndex - length + i - 1] - curLog[runDayIndex - length + i];
					if (diff == 0)
						continue;

					std::string tradeText = diff < 0 ? "[BUY] " : "[SELL] ";
					tradeText += portfolio.stocks[j].symbol;
					tradeText += " x";
					tradeText += std::to_string(diff * (diff < 0 ? -1 : 1));
					ImGui::Text(tradeText.c_str());
				}
			}

			ImGui::PopStyleColor();
		}
	}
}