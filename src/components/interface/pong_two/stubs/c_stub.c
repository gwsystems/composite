#include <pong_two.h>

void call_intern(int u1, int u2, int u3, int u4, int *u5, int *u6);
void call_two_intern(int u1, int u2, int u3, int u4, int *u5, int *u6);
void call_three_intern(int u1, int u2, int u3, int u4, int *u5, int *u6);
void call_four_intern(int u1, int u2, int u3, int u4, int *u5, int *u6);

void call_arg_intern(int p1, int u1, int u2, int u3, int *u4, int *u5);
void call_args_intern(int p1, int p2, int p3, int p4, int *u1, int *u2);

void
call(void)
{
	int unused;

	return call_intern(unused, unused, unused, unused, &unused, &unused);
}

void
call_two(void)
{
	int unused;

	return call_two_intern(unused, unused, unused, unused, &unused, &unused);
}

void
call_three(void)
{
	int unused;

	return call_three_intern(unused, unused, unused, unused, &unused, &unused);
}

void
call_four(void)
{
	int unused;

	return call_four_intern(unused, unused, unused, unused, &unused, &unused);
}

void
call_arg(int p1)
{
	int unused;

	return call_arg_intern(p1, unused, unused, unused, &unused, &unused);
}

void
call_args(int p1, int p2, int p3, int p4)
{
	int unused;

	return call_args_intern(p1, p2, p3, p4, &unused, &unused);
}
