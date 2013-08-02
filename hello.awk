BEGIN {
	extension("./cawk.so", "dlload")

	load_shlib("./hello.so")
	load_shlib("libm.so")
	load_shlib("libc.so.6")

	c_func_resist("hello.so", "hello", "v")
	c_func_resist("hello.so", "square", "d", "d")
	c_func_resist("hello.so", "square", "i", "di")
	c_func_resist("libm.so", "abs", "i", "i")
	c_func_resist("libm.so", "fabs", "d", "d")
	c_func_resist("libc.so.6", "malloc", "i", "i")
	c_func_resist("libc.so.6", "random", "i")

	hello()
	print square(-3)
	print cut_p_disit(3.141593, 3)
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
