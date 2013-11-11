#!/usr/local/bin/gawk-4.1.0 -f

@load "./cawk.so"

BEGIN {
	# extension("./cawk.so")

	load_shlib("./hello.so")
	load_shlib("libm.so")
	load_shlib("libc.so.6")

	resist_func("./hello.so",  "hello",  "v", "v")
	resist_func("./hello.so",  "hello2", "v", "$")
	resist_func("./hello.so",  "hello3", "$", "v")
	resist_func("./hello.so",  "square", "d", "d")
	resist_func("./hello.so",  "square", "i", "di")
	resist_func("libm.so",   "abs",    "i", "i")
	resist_func("libm.so",   "fabs",   "d", "d")
	resist_func("libc.so.6", "malloc", "i", "i")
	resist_func("libc.so.6", "random", "i", "v")

	hello()
	hello2("AWK")
	print hello3()
	print square(-3)
	#print cut_p_disit(3.141593, 3)
	print abs(-89)
	print fabs(-3.14)
	printf "%x\n", malloc(8)
	print random()
	foo()

	close_shlib("./hello.so")
	#hello()
}

function foo()
{
	print "foo"
}
