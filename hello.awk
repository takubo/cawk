BEGIN {
	extension("./cawk.so", "dlload")
	load_shlib("./hello.so")
	c_func_resist("hello.so", "hello", "v")
	c_func_resist("hello.so", "square", "dd")
	hello()
	print square(-3)
	load_shlib("libm.so")
	c_func_resist("libm.so", "abs", "ii")
	c_func_resist("libm.so", "fabs", "dd")
	print abs(-89)
	print fabs(-3.14)
	foo()
}

function foo()
{
	print "foo"
}
