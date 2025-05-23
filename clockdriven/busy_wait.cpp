#include "busy_wait.h"

#include <chrono>

void busy_wait(unsigned int millisec)
{	
	auto stop = std::chrono::high_resolution_clock::now();
	stop += std::chrono::milliseconds(millisec);
	
	while (std::chrono::high_resolution_clock::now() < stop)
	{
	// busy wait
	}
}



