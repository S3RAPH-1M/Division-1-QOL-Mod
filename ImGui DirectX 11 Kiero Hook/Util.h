#pragma once
#include <vector>
#include <Windows.h>

namespace util
{
    namespace hooks
    {
        void Init();

        void DisableHooks();
        void EnableHooks();
    };

    namespace log
    {
        void Init();

    };
    namespace math
    {
        double CatmullRomInterpolate(double y0, double y1, double y2, double y3, double mu);
    }
}