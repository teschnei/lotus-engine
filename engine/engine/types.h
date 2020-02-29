#pragma once
#include <chrono>

using namespace std::literals::chrono_literals;
namespace lotus
{
    using sim_clock = std::chrono::steady_clock;
    using time_point = sim_clock::time_point;
    using duration = sim_clock::duration;

    enum class Lifetime
    {
        Short,
        Long
    };
}
