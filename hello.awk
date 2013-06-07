BEGIN {
	extension("./cawk.so", "dlload")
	load_shlib("./hello.so")
	c_func_resist("hello.so", "hello", "v")
	#c_func_resist("hello", "")
	hello("world")
}

function foo()
{
	print "bye"
}
