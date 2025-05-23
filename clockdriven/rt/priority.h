#ifndef RT_PRIORITY_H
#define RT_PRIORITY_H

#include <thread>
#include <string>
#include <stdexcept>
#include <ostream>

namespace rt
{

class priority
{
	public:
		static const priority rt_max;
		static const priority rt_min;
		static const priority not_rt;
	
		priority();
	
		bool is_rt() const;

		priority & operator ++();
		priority & operator --();

		priority operator ++(int);
		priority operator --(int);

		priority & operator +=(unsigned int n);
		priority & operator -=(unsigned int n);
		
		int operator -(const priority & p) const;
	
		bool operator <(const priority & p) const;
		bool operator <=(const priority & p) const;
		bool operator >(const priority & p) const;
		bool operator >=(const priority & p) const;
		bool operator ==(const priority & p) const;
		bool operator !=(const priority & p) const;

	private:
		priority(unsigned int value);

		unsigned int value;

		friend std::ostream& operator <<(std::ostream& stream, const priority & p);
};

std::ostream& operator <<(std::ostream& stream, const priority & p);

class permission_error : public std::runtime_error
{
	public:
		explicit permission_error(const std::string&  what_arg);
};

priority get_priority(const std::thread & th);

void set_priority(std::thread & th, const priority & p); // throw (permission_error)

namespace this_thread
{
priority get_priority();

void set_priority(const priority & p); // throw (permission_error)

class scoped_priority
{
	public:
		scoped_priority(const priority & p); // throw (permission_error)
		~scoped_priority();
		
	private:
		priority ret_prio;
};
}

// ...............................................................................................

inline priority operator +(priority a, unsigned int n)
{
	return a += n;
}

inline priority operator +(unsigned int n, priority a)
{
	return a += n;
}

inline priority operator -(priority a, unsigned int n)
{
	return a -= n;
}

inline priority operator -(unsigned int n, priority a)
{
	return a -= n;
}
	
inline priority::priority() :value(0)
{
}

inline bool priority::is_rt() const
{
	return value > 0;
}

inline priority & priority::operator ++()
{
	if (value < rt_max.value)
		++value;

	return *this;
}

inline priority & priority::operator --()
{
	if (value > 0)
		--value;

	return *this;
}

inline priority priority::operator ++(int)
{
	priority ret(*this);
	++(*this);
	return ret;
}

inline priority priority::operator --(int)
{
	priority ret(*this);
	--(*this);
	return ret;
}

inline priority & priority::operator +=(unsigned int n)
{
	value += n;
	
	if (value > rt_max.value)
		value = rt_max.value;
	
	return *this;
}

inline priority & priority::operator -=(unsigned int n)
{
	if (value > n)
		value -= n;
	else
		value = 0;
	
	return *this;
}

inline int priority::operator -(const priority & p) const
{
	int va = value;
	int vb = p.value;
	return va - vb;
}

inline bool priority::operator <(const priority & p) const
{
	return value < p.value;
}

inline bool priority::operator <=(const priority & p) const
{
	return value <= p.value;
}

inline bool priority::operator >(const priority & p) const
{
	return value > p.value;
}

inline bool priority::operator >=(const priority & p) const
{
	return value >= p.value;
}

inline bool priority::operator ==(const priority & p) const
{
	return value == p.value;
}

inline bool priority::operator !=(const priority & p) const
{
	return value != p.value;
}

inline permission_error::permission_error(const std::string&  what_arg) : std::runtime_error(what_arg)
{
}

namespace this_thread
{

inline scoped_priority::scoped_priority(const priority & p) : ret_prio(this_thread::get_priority())
{
	this_thread::set_priority(p);
}
		
inline scoped_priority::~scoped_priority()
{
	this_thread::set_priority(ret_prio);
}

}

}

#endif

