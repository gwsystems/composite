#include "cxx.h"

extern "C" {
#include <cos_defkernel_api.h>
}

#include <typeinfo> /*for 'typeid'*/

void
basic_test()
{
	cl s;
	s.put_i(100);
	cout << "Basic class test " << s.get_i() << endl;
}

void
template_test()
{
	Mypair <int> myobject (75, 100);
	cout << "Template test " << myobject.getmax() << endl;
}

void
overload_test()
{
	Getdata gd;
	cout << "Overload test " << gd.getdata(100) << ' ' << gd.getdata(2.7182818) << ' ' << gd.getdata('$') << endl;
}

void
inherit_test()
{
	Rectangle rect;
	Triangle trgl;
	Polygon * ppoly1 = &rect;
	Polygon * ppoly2 = &trgl;
	ppoly1->set_values (4,50);
	ppoly2->set_values (4,50);
	cout << "Inheritance test rect " << rect.area() << " trgl " << trgl.area() << endl;
}

void
new_delete_test()
{
	cl *s;
	int *p, i;
	cout << "new test " << endl;
	s = new cl(50);
	s->put_i(100);
	cout << "delete test " << endl;
	delete s;
	cout << "new [] test >>" << endl;
	p = new int[100];
	for(i = 0; i<100; i++)
		p[i] = -1;
	delete[] p;
	cout << ">> delete [] test" << endl;
}

void
polymorphism_test()
{
	Polygon *p1 = new Rectangle(5, 40);
	Polygon *p2 = new Triangle(40, 5);
	cout << "polymorphism test rect " << p1->retarea() << " trgl " <<  p2->retarea() << endl;
	delete p1;
	delete p2;
}

void
my_function(Person* my_a)
{
		Employee* my_b = dynamic_cast<Employee*>(my_a); /*cast will be successful only for B type objects.*/
		if (my_b) my_b->methodSpecificToB();
		else cout << "Bad cast" << endl;
}

void
rtti_test()
{
	Person person;
	Employee employee;
	Person* ptr = &employee;
	Person& ref = employee;

	cout << "rtti test >>> typeid" << endl;
	cout << typeid(person).name() << endl;   /*Person (statically known at compile-time)*/
	cout << typeid(employee).name() << endl; /*Employee (statically known at compile-time)*/
	cout << typeid(ptr).name() << endl;      /*Person* (statically known at compile-time)*/
	cout << typeid(*ptr).name() << endl;     /*Employee (looked up dynamically at run-time*/
	cout << typeid(ref).name() << endl;      /*Employee (references can also be polymorphic)*/

	Person *a1, *a2;       /*pointers to base class */
	a1 = new Employee();   /*Pointer to Employee object*/
	a2 = new Person();     /*Pointer to Person object*/

	cout << "rtti test >>> dynamic_cast" << endl;
	my_function(a1);
	delete a1;
	my_function(a2);
	delete a2;
}

void
vector_test()
{
	vector<int> first;
	vector<int> second(4, 1000);
	vector<int> third(second.begin(), second.end());
	vector<int> fourth(third);
	int a[] = {16, -2, 77, 497654};
	unsigned int i;
	vector<int> fifth(a, a + sizeof(a) / sizeof(int));
	cout << "vector test >>>>" << endl;
	cout << "size " << fifth.size() << " capacity " << fifth.capacity() << endl;
	cout << "contents";
	for(vector<int>::iterator it = fifth.begin(); it != fifth.end(); it++) {
		cout << ' ' << *it;
	}
	for(i = 0; i<second.size(); i++) {
		fifth.push_back(second[i]);
	}
	cout << "\nsize " << fifth.size() << " capacity %d" <<  fifth.capacity() << endl;
	cout << "contents";
	for(vector<int>::iterator it = fifth.begin(); it != fifth.end(); it++) {
		cout << ' ' <<  *it;
	}
	for(i = 0; i<2048; i++) fifth.push_back(5);
	cout << "\nsize " << fifth.size() << " capacity " <<  fifth.capacity() << endl;
}

void
string_test()
{
	char *s = "world";
	string s1 = "hello ";
	string s2(s);
	string s3 = s1 + s2;
	cout << "string test >>>>" << endl;
	cout << s3.c_str() << endl;
	s3.erase(6, 5);
	cout << s3.c_str() << endl;
}

Global_foo foo;
Global_bar bar;

extern "C" void
cos_init(void *args)
{
	cout << "=========CXX TEST=========" << endl;
	basic_test();
	template_test();
	overload_test();
	inherit_test();
	new_delete_test();
	polymorphism_test();
	rtti_test();
	vector_test();
	string_test();
	cout << "==========CXX TEST DONE=========" << endl;

	SPIN();
}
