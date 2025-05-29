#include "busy_wait.h"

#include <chrono>

// cycles until a maximum amount of milliseconds or a maximum amount of cycles espires
static unsigned int busy_wait_impl(unsigned int max_millisec, unsigned int max_cycles)
{
	volatile unsigned int cycles = 0;

	auto stop_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(max_millisec);

	while (cycles < max_cycles && std::chrono::high_resolution_clock::now() < stop_time)
		++cycles;
		
	return cycles;
}

// estimation: the number of busy_wait_impl() cycles that correspond to a millisec
static unsigned int millisec_cycles = 0;

// estimates millisec_cycles
void busy_wait_init()
{
	unsigned int sum = 0;
	unsigned int i;
	
	for (i=0; i<10; ++i)
	{
		sum += busy_wait_impl(100, 0xFFFFFFFF);
	}
	
	millisec_cycles = sum / 10 / 100;
}

// does a busy wait pause
void busy_wait(unsigned int millisec)
{
	busy_wait_impl(100000, millisec * millisec_cycles);
}


