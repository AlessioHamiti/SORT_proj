#include "executive.h"
#include <iostream>

#include "busy_wait.h"

void task0()
{
	std::cout << "Sono il task n.0" << std::endl;
	busy_wait(15);
}

void task1()
{
	std::cout << "Sono il task n.1" << std::endl;
	busy_wait(6);
}
void task2()
{
	std::cout << "Sono il task n.2" << std::endl;
	busy_wait(18);
}

void task3()
{
	std::cout << "Sono il task n.3" << std::endl;
	busy_wait(17);
}

void task4()
{
	static unsigned count = 0;

	std::cout << "Sono il task n.4" << std::endl;
	
	if (++count % 5 == 0)
		busy_wait(31);
	else
		busy_wait(28);
}

void task5()
{
	std::cout << "Sono il task n.5" << std::endl;
	busy_wait(8);
}

int main()
{
	Executive exec(6, 5);

	exec.set_periodic_task(0, task0, 2);
	exec.set_periodic_task(1, task1, 1);
	exec.set_periodic_task(2, task2, 2);
	exec.set_periodic_task(3, task3, 2);
	exec.set_periodic_task(4, task4, 3);
	exec.set_periodic_task(5, task5, 1);
	
	exec.add_frame({0,1,2});
	exec.add_frame({3,4});
	exec.add_frame({0,3});
	exec.add_frame({1,4,5});
	exec.add_frame({0,2});
	exec.add_frame({1,5,2});
	
	exec.start();
	exec.wait();
	
	return 0;
}
