module;

#include <chrono>

export module lotus:util.types;

export using namespace std::literals::chrono_literals;
export namespace lotus
{
using sim_clock = std::chrono::steady_clock;
using time_point = sim_clock::time_point;
using duration = sim_clock::duration;

enum class Lifetime
{
    Short,
    Long
};

} // namespace lotus
