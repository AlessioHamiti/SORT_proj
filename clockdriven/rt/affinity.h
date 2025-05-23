#ifndef RT_AFFINITY_H
#define RT_AFFINITY_H

#include <thread>
#include <bitset>

namespace rt
{

typedef std::bitset<32> affinity;

affinity get_affinity(const std::thread & th);
void set_affinity(std::thread & th, const affinity & a);

namespace this_thread
{
affinity get_affinity();
void set_affinity(const affinity & a);
}

}

#endif

