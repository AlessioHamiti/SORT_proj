#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#else
#pragma message ("affinity control not implemented")
#endif

#include <pthread.h>
#include <sched.h>
#include <cstring>

#include "priority.h"
#include "affinity.h"

namespace rt
{

const priority priority::rt_max(sched_get_priority_max(SCHED_FIFO) - sched_get_priority_min(SCHED_FIFO) + 1);
const priority priority::rt_min(1);
const priority priority::not_rt;

priority::priority(unsigned int value) : value(value)
{
}

std::ostream& operator <<(std::ostream& stream, const priority & p)
{
		stream << p.value;
		return stream;
}

namespace detail
{

static priority get_priority(pthread_t pthread_id)
{
	int policy = SCHED_OTHER;
	struct sched_param param = {};

	pthread_getschedparam(pthread_id, &policy, &param);

	if (policy == SCHED_FIFO)
		return priority::rt_min + (param.sched_priority - sched_get_priority_min(SCHED_FIFO));
	else
		return priority::not_rt;
}

static void set_priority(pthread_t pthread_id, const priority & p)
{
	struct sched_param param = {};
	int res = 0;

	if (p.is_rt())
	{
		param.sched_priority = (p - priority::rt_min) + sched_get_priority_min(SCHED_FIFO);
		
		res = pthread_setschedparam(pthread_id, SCHED_FIFO, &param);
	}
	else
		res = pthread_setschedparam(pthread_id, SCHED_OTHER, &param);
		
	if (res != 0)
	{
		char msg[30];
		throw permission_error(strerror_r(res, msg, 30));
	}
}

static affinity get_affinity(pthread_t pthread_id)
{
	affinity a;

#ifdef __linux__
	cpu_set_t * cpuset = CPU_ALLOC(a.size());
	
	pthread_getaffinity_np(pthread_id, CPU_ALLOC_SIZE(a.size()), cpuset);

	for (size_t i = 0; i < a.size(); ++i)
		a[i] = CPU_ISSET_S(i, CPU_ALLOC_SIZE(a.size()), cpuset);

	CPU_FREE(cpuset);
#else
	a.set();
#endif
	return a;
}

static void set_affinity(pthread_t pthread_id, const affinity & a)
{
#ifdef __linux__
	cpu_set_t * cpuset = CPU_ALLOC(a.size());

	CPU_ZERO_S(CPU_ALLOC_SIZE(a.size()), cpuset);

	for (size_t i = 0; i < a.size(); ++i)
		if (a[i])
			CPU_SET_S(i, CPU_ALLOC_SIZE(a.size()), cpuset);

	pthread_setaffinity_np(pthread_id, CPU_ALLOC_SIZE(a.size()), cpuset);

	CPU_FREE(cpuset);
#endif
}

}

priority get_priority(const std::thread & th)
{
	return detail::get_priority(const_cast<std::thread &>(th).native_handle());
}

void set_priority(std::thread & th, const priority & p)
{
	detail::set_priority(th.native_handle(), p);
}

affinity get_affinity(const std::thread & th)
{
	return detail::get_affinity(const_cast<std::thread &>(th).native_handle());
}

void set_affinity(std::thread & th, const affinity & a)
{
	detail::set_affinity(th.native_handle(), a);
}


namespace this_thread
{

priority get_priority()
{
	return detail::get_priority(pthread_self());
}

void set_priority(const priority & p)
{
	detail::set_priority(pthread_self(), p);
}

affinity get_affinity()
{
	return detail::get_affinity(pthread_self());
}

void set_affinity(const affinity & a)
{
	detail::set_affinity(pthread_self(), a);
}

}

}

