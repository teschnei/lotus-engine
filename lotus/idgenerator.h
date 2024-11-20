#pragma once

#include <concepts>

namespace lotus
{
    template<typename T, std::integral Type>
    class IDGenerator
    {
    private:
        static Type count;
    public:
        template<typename U>
        static const Type GetNewID()
        {
            static const Type static_count = count++;
            return static_count;
        }
    };

    template<typename T, std::integral Type> Type IDGenerator<T, Type>::count = 0;
}