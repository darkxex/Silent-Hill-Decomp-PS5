#include "elevation/elevation.hpp"

extern "C" {
int ps5_elevate_lapy(void)
{
    return static_cast<int>(elevation::request(elevation::Capability::filesystem));
}
}
