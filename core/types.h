#pragma once
#include <chrono>

namespace lotus
{
    using namespace std::literals::chrono_literals;
    using sim_clock = std::chrono::steady_clock;
    using time_point = sim_clock::time_point;
    using duration = sim_clock::duration;
}
