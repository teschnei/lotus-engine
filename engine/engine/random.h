#pragma once

#include <random>
#include <array>

namespace lotus
{
    class random
    {
    public:
        template <typename T>
        static inline typename std::enable_if<std::is_integral<T>::value, T>::type
            GetRandomNumber(T min, T max)
        {
            if (min == max - 1 || max == min)
            {
                return min;
            }
            std::uniform_int_distribution<T> dist(min, max);
            return dist(mt());
        }

        template<typename T>
        static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type
            GetRandomNumber(T min, T max)
        {
            if (min == max)
            {
                return min;
            }
            else if (max < min)
            {
                std::uniform_real_distribution<T> dist(max, min);
                return dist(mt());
            }
            else
            {
                std::uniform_real_distribution<T> dist(min, max);
                return dist(mt());
            }
        }

    private:
        static std::mt19937& mt()
        {
            static thread_local std::mt19937 e{};
            if (!initialized)
            {
                initialized = true;
                seed();
            }
            return e;
        }

        static void seed(void)
        {
            std::array<uint32_t, std::mt19937::state_size> seed_data;
            std::random_device rd;
            std::generate(seed_data.begin(), seed_data.end(), std::ref(rd));
            std::seed_seq seq(seed_data.begin(), seed_data.end());
            mt().seed(seq);
        }

        static inline thread_local bool initialized{ false };
    };
}
