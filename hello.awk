BEGIN {
	extension("./cawk.so", "dlload")
	#load_shlib("./hello.so")
	#c_func_resist("hello.so", "hello", "v")
	#c_func_resist("hello.so", "bye", "v")
	#c_func_resist("hello.so", "abs", "dd")
	#c_func_resist("hello", "")
	#hello()
	#bye()
	load_shlib("libm.so")
	c_func_resist("libm.so", "abs", "ii")
	print abs(-89)
	c_func_resist("libm.so", "fabs", "dd")
	print fabs(-3.14)
}

function foo()
{
	print "bye"
}
