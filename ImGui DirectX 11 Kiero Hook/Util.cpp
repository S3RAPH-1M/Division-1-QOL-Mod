#include "Util.h"
#include "Main.h"

double util::math::CatmullRomInterpolate(double y0, double y1, double y2, double y3, double mu)
{
    double mu2 = mu * mu;
    double a0 = -0.5 * y0 + 1.5 * y1 - 1.5 * y2 + 0.5 * y3;
    double a1 = y0 - 2.5 * y1 + 2 * y2 - 0.5 * y3;
    double a2 = -0.5 * y0 + 0.5 * y2;
    double a3 = y1;

    return a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
}
