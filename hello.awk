BEGIN {
	extension("./cawk.so", "dlload")

	load_shlib("./hello.so")
	load_shlib("libm.so")
	load_shlib("libc.so.6")

	c_func_resist("hello.so", "hello", "v")
	c_func_resist("hello.so", "square", "dd")
	c_func_resist("libm.so", "abs", "ii")
	c_func_resist("libm.so", "fabs", "dd")
	c_func_resist("libc.so.6", "malloc", "ii")
	c_func_resist("libc.so.6", "random", "i")

	hello()
	print square(-3)
	print abs(-89)
	print fabs(-3.14)
	printf "%x\n", malloc(8)
	print random()
	foo()
}

function foo()
{
	print "foo"
}
