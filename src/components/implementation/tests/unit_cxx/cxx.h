#ifndef CXX_UNIT_H
#define CXX_UNIT_H

#include <vector>
#include <algorithm>
#include <set>
#include <queue>
#include <map>
#include <string>
#include <iostream>
using namespace std;
#ifdef __cplusplus
extern "C" {
#include <llprint.h>
#endif

#ifdef __cplusplus
}
#endif

class Global_foo {
      public:
	Global_foo();
	~Global_foo();
};

Global_foo::Global_foo() { printc("Global foo constructor\n"); }

Global_foo::~Global_foo() { cout << "Global foo destructor" << endl; }

class Global_bar {
      public:
	Global_bar();
};

Global_bar::Global_bar() { printc("Global bar constructor\n"); }

class cl {
	int i;

      public:
	cl(void);
	cl(int a);
	~cl();
	int  get_i();
	void put_i(int j);
};

cl::cl(void) { i = 0; }

cl::cl(int a)
{
	i = a;
	cout << "create object " << i << endl;
}

cl::~cl() { cout << "destroy object " << i << endl; }

int
cl::get_i()
{
	return i;
}

void
cl::put_i(int j)
{
	i = j;
}

template<class T>
class Mypair {
	T a, b;

      public:
	Mypair(T first, T second)
	{
		a = first;
		b = second;
	}
	T getmax();
};

template<class T>
T
Mypair<T>::getmax()
{
	T retval;
	retval = a > b ? a : b;
	return retval;
}

class Getdata {
      public:
	int    getdata(int i);
	double getdata(double f);
	char   getdata(char c);
};

int
Getdata::getdata(int i)
{
	return i;
}

double
Getdata::getdata(double f)
{
	return f;
}

char
Getdata::getdata(char c)
{
	return c;
}

class Polygon {
      protected:
	int width, height;

      public:
	Polygon(int a, int b)
	  : width(a)
	  , height(b)
	{
	}
	virtual ~Polygon() {}
	virtual int area(void) = 0;
	void        set_values(int a, int b);
	int         retarea();
};

void
Polygon::set_values(int a, int b)
{
	width  = a;
	height = b;
}

int
Polygon::retarea(void)
{
	return this->area();
}

class Rectangle : public Polygon {
      public:
	Rectangle(int a = 1, int b = 1)
	  : Polygon(a, b)
	{
	}
	virtual ~Rectangle() {}
	int area(void);
};

int
Rectangle::area()
{
	return width * height;
}

class Triangle : public Polygon {
      public:
	Triangle(int a = 2, int b = 2)
	  : Polygon(a, b)
	{
	}
	virtual ~Triangle() {}
	int area(void);
};

int
Triangle::area()
{
	return width * height / 2;
}

class Person {
      public:
	virtual ~Person() {}
	void methodSpecificToA();
};

void
Person::methodSpecificToA()
{
	cout << "Method specific for A was invoked" << endl;
}

class Employee : public Person {
      public:
	virtual ~Employee() {}
	void methodSpecificToB();
};

void
Employee::methodSpecificToB()
{
	cout << "Method specific for B was invoked" << endl;
}

#endif /* CXX_UNIT_H */
