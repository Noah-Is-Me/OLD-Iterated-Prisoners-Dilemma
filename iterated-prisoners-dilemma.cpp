
#include <stdio.h>
#include <iostream>
#include <array>
#include <unordered_map>
#include <chrono>
#include <thread>

#include "strategy.h"

void runIteration(Strategy &s1, Strategy &s2, double miscommunicationRate, double misexecutionRate)
{
    Move s1Move = getFail(s1.nextMove, misexecutionRate);
    Move s2Move = getFail(s2.nextMove, misexecutionRate);

    s1.onMove(getFail(s2Move, miscommunicationRate));
    s2.onMove(getFail(s1Move, miscommunicationRate));

    s1.points += pointMatrix[s1Move][s2Move];
    s2.points += pointMatrix[s2Move][s1Move];
}

template <std::size_t N1, std::size_t N2>
void runMatchup(int u, int i, int j, std::array<StrategyData, N1> &strategies, const std::array<double, N2> &miscommunicationRates, const std::array<double, N2> &misexecutionRates, int iterationCount)
{
    StrategyData &sd1 = strategies[i];
    StrategyData &sd2 = strategies[j];

    std::unique_ptr<Strategy> s1_ = sd1.constructor();
    std::unique_ptr<Strategy> s2_ = sd2.constructor();

    Strategy &s1 = *s1_;
    Strategy &s2 = *s2_;

    s1.reset();
    s2.reset();

    for (int k = 0; k < iterationCount; k++)
    {
        runIteration(s1, s2, miscommunicationRates[u], misexecutionRates[u]);
    }

    sd1.addPoints(u, s1.points);
    sd2.addPoints(u, s2.points);
}

void joinThreads(std::vector<std::thread> &threads)
{
    for (std::thread &t : threads)
    {
        t.join();
    }
    threads.clear();
}

template <std::size_t N1, std::size_t N2>
void runRound(int u, std::array<StrategyData, N1> &strategies, const std::array<double, N2> &miscommunicationRates, const std::array<double, N2> &misexecutionRates, int iterationCount, int parallelProcessMatchups, int maxMatchupThreads, int giveRoundUpdates)
{
    auto roundStart = std::chrono::high_resolution_clock::now();

    /* int totalMatchups = (strategies.size() * (strategies.size() + 1)) / 2;
    runThreads(totalMatchups, strategies.size(), strategies.size(), parallelProcessMatchups, maxMatchupThreads,
               runMatchup<N1, N2>,
               std::ref(strategies), iterationCount, std::cref(miscommunicationRates), std::cref(misexecutionRates));
    */

    std::vector<std::thread> threads;

    for (int i = 0; i < strategies.size(); i++)
    {
        for (int j = i; j < strategies.size(); j++)
        {
            if (parallelProcessMatchups)
            {
                if (threads.size() >= maxMatchupThreads)
                {
                    joinThreads(threads);
                }

                threads.push_back(std::thread(runMatchup<N1, N2>, u, i, j, std::ref(strategies), std::cref(miscommunicationRates), std::cref(misexecutionRates), iterationCount));
            }
            else
            {
                runMatchup(u, i, j, strategies, miscommunicationRates, misexecutionRates, iterationCount);
            }
        }
    }

    if (parallelProcessMatchups)
        joinThreads(threads);

    if (giveRoundUpdates && u % 5 == 0)
    {
        int totalRounds = miscommunicationRates.size();
        auto roundEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> roundDuration = roundEnd - roundStart;
        double estimatedTime = roundDuration.count() * (totalRounds - (u + 1));
        std::cout << "[NOTICE] Round " << u << " complete, Time: " << roundDuration.count() << " seconds\n"
                  << "[NOTICE] Estimated time: " << estimatedTime << " seconds  (" << estimatedTime / 60 << " minutes)" << std::endl;
    }
}

/* template <typename Function, typename... Args>
void runThreads(int iterations, int iterator1, int iterator2, bool useParallelProcessing, int maxThreads, Function &&func, Args &&...args)
{
    std::vector<std::thread> threads;

    int i1 = 0;
    int i2 = 0;

    for (int i = 0; i < iterations; i++)
    {
        if (useParallelProcessing)
        {
            if (threads.size() >= maxThreads)
            {
                joinThreads(threads);
            }

            threads.push_back(std::thread(
                std::forward<Function>(func), i, i1, i2++,
                std::forward<Args>(args)...));
        }
        else
        {
            std::forward<Function>(func)(i, i1, i2++, std::forward<Args>(args)...);
        }

        if (i1 % iterator1 == 0)
            i1 = 0;

        if (i2 % iterator2 == 0)
        {
            i1++;
            i2 = 0;
        }
    }

    if (useParallelProcessing)
        joinThreads(threads);
}
*/

template <std::size_t N1, std::size_t N2>
void outputData(const std::array<StrategyData, N1> &strategies, const std::array<double, N2> &miscommunicationRates, const std::array<double, N2> &misexecutionRates)
{
    std::string fullData = "";

    for (const StrategyData &s : strategies)
    {
        std::string data = s.name + ",";
        for (int i = 0; i < N2; i++)
        {
            data += std::to_string(miscommunicationRates[i]) + "," + std::to_string(misexecutionRates[i]) + "," + std::to_string(s.averagePoints[i]) + ",";
        }

        fullData += data + "\n";
        std::cout << data << std::endl;
    }
}

template <typename T>
std::unique_ptr<Strategy> makeConstructor()
{
    return std::make_unique<T>();
}

template <typename... Types>
std::array<std::function<std::unique_ptr<Strategy>()>, sizeof...(Types)> makeConstructors()
{
    return std::array<std::function<std::unique_ptr<Strategy>()>, sizeof...(Types)>{
        &makeConstructor<Types>...};
}

int main()
{
    auto totalStart = std::chrono::high_resolution_clock::now();

    const int cores = std::thread::hardware_concurrency();

    const int maxRoundThreads = cores;
    const int maxMatchupThreads = cores;

    const bool parallelProcessRounds = true;
    const bool parallelProcessMatchups = false;

    const bool giveRoundUpdates = false;

    const double startingMiscommunicationRate = 0.00;
    const double startingMisexecutionRate = 0.00;

    const double miscommunicationRateIncrement = 0.01;
    const double misexecutionRateIncrement = 0.00;

    const int totalRounds = 100 + 1;
    const int iterationCount = 10000;

    std::array<double, totalRounds> miscommunicationRates;
    std::array<double, totalRounds> misexecutionRates;
    for (int i = 0; i < totalRounds; i++)
    {
        miscommunicationRates[i] = startingMiscommunicationRate + miscommunicationRateIncrement * i;
        misexecutionRates[i] = startingMisexecutionRate + misexecutionRateIncrement * i;
    }

    // All Strategies
    // auto strategyConstructors = makeConstructors<
    //     TitForTat,
    //     ForgivingTitForTat,
    //     AlwaysDefect,
    //     AlwaysCooperate,
    //     Random,
    //     ProbabilityCooperator,
    //     ProbabilityDefector,
    //     SuspiciousTitForTat,
    //     GenerousTitForTat,
    //     GradualTitForTat,
    //     ImperfectTitForTat,
    //     TitForTwoTats,
    //     TwoTitsForTat,
    //     GrimTrigger,
    //     Pavlov>();

    // TitForTat vs ForgivingTitForTat
    // auto strategyConstructors = makeConstructors<
    //     ForgivingTitForTat,
    //     TitForTat,
    //     TitForTat,
    //     TitForTat,
    //     TitForTat,
    //     TitForTat,
    //     TitForTat,
    //     TitForTat,
    //     TitForTat,
    //     TitForTat,
    //     Random>();

    // Nice Strategies
    // auto strategyConstructors = makeConstructors<
    //     AlwaysCooperate,
    //     ForgivingTitForTat,
    //     TitForTat,
    //     ProbabilityCooperator,
    //     GenerousTitForTat,
    //     TitForTwoTats,
    //     Random>();

    // Mean Strategies
    // auto strategyConstructors = makeConstructors<
    //     GradualTitForTat,
    //     GrimTrigger,
    //     SuspiciousTitForTat,
    //     ProbabilityDefector,
    //     TwoTitsForTat,
    //     Random>();

    // Nice Strategies Infiltration
    auto strategyConstructors = makeConstructors<
        AlwaysDefect,
        AlwaysCooperate,
        ForgivingTitForTat,
        TitForTat,
        ProbabilityCooperator,
        GenerousTitForTat,
        TitForTwoTats,
        Random>();

    std::array<StrategyData, strategyConstructors.size()> strategies;
    for (int i = 0; i < strategies.size(); i++)
    {
        strategies[i].name = strategyConstructors[i]()->name;
        strategies[i].totalPoints.assign(totalRounds, 0);
        strategies[i].averagePoints.assign(totalRounds, 0);
        strategies[i].constructor = strategyConstructors[i];
    }

    /* runThreads(totalRounds, parallelProcessRounds, maxRoundThreads, runRound<strategies.size(), totalRounds>,
               std::ref(strategies), iterationCount, std::cref(miscommunicationRates), std::cref(misexecutionRates), parallelProcessMatchups, giveRoundUpdates, totalRounds, maxMatchupThreads);
    */

    std::vector<std::thread> threads;

    for (int u = 0; u < totalRounds; u++)
    {
        if (parallelProcessRounds)
        {
            if (threads.size() >= maxRoundThreads)
            {
                joinThreads(threads);
            }

            threads.push_back(std::thread(
                runRound<strategies.size(), totalRounds>, u, std::ref(strategies), std::cref(miscommunicationRates), std::cref(misexecutionRates), iterationCount, parallelProcessMatchups, maxMatchupThreads, giveRoundUpdates));
        }
        else
        {
            runRound(u, strategies, miscommunicationRates, misexecutionRates, iterationCount, parallelProcessMatchups, maxMatchupThreads, giveRoundUpdates);
        }
    }

    if (parallelProcessRounds)
        joinThreads(threads);

    for (StrategyData &strategy : strategies)
    {
        for (int i = 0; i < totalRounds; i++)
        {
            strategy.averagePoints[i] = 1.0 * strategy.totalPoints[i] / (strategies.size() + 1);
        }
    }

    outputData(strategies, miscommunicationRates, misexecutionRates);

    auto totalEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> totalDuration = totalEnd - totalStart;
    std::cout << "[NOTICE] Total time taken: " << totalDuration.count() << " seconds\n"
              << "[NOTICE] Average round time : " << totalDuration.count() / totalRounds << " seconds " << std::endl;

    return 0;
}
