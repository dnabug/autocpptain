#pragma once

namespace autocaptain
{

template <typename T>
void update_average(T* o, T n, unsigned int entries)
{
    *o = (*o * entries + n) / (entries + 1);
}

}
