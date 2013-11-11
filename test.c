#include <stdio.h>

void init(void)
{
	puts("start");
}

void fini(void)
{
	puts("end");
}

void
hello(void)
{
	puts("hello");
}

void
hello2(char *name)
{
	printf("hello, %s\n", name);
}

char *
hello3(void)
{
	return "bye";
}

double
square(double n)
{
	return n * n;
}

int
cut_p_disit(double n, int c)
{
	int ans;

	ans = (int)(n * c * 10) % 10;
	return ans;
}
