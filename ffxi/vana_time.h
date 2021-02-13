#pragma once

#include "engine/types.h"
#include <chrono>

struct FFXITime
{
    using milliseconds = std::chrono::duration<long long, std::ratio<60, 25 * 60 * 1000>>;
    using seconds = std::chrono::duration<long long, std::ratio<60, 25 * 60>>;
    using minutes = std::chrono::duration<long long, std::ratio<60, 25>>;
    using hours = std::chrono::duration<long long, std::ratio<60 * 60, 25>>;
    using days = std::chrono::duration<long long, std::ratio<60 * 1440, 25>>;
    using weeks = std::chrono::duration<long long, std::ratio<60 * 11520, 25>>;
    using months = std::chrono::duration<long long, std::ratio<60 * 43200, 25>>;
    using years = std::chrono::duration<long long, std::ratio<60 * 518400, 25>>;

    using duration = milliseconds;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<FFXITime>;
    static const bool is_steady = false;

    static time_point now() noexcept
    {
        using namespace std::chrono;
        return time_point{duration_cast<duration>(system_clock::now().time_since_epoch()) - BaseDate};
    }

    static inline duration vana_time() noexcept
    {
        return now().time_since_epoch();
    }

private:
    static constexpr duration BaseDate = std::chrono::duration_cast<duration>(std::chrono::seconds(1009810800));
};