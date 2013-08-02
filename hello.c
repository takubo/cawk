#include <stdio.h>

void
hello(void)
{
	puts("hello");
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

	ans = (n * c * 10) % 10;
	return ans;
}
