#!/usr/local/bin/gawk-4.1.0 -f

@load "./cawk.so"

BEGIN {
	#load_shlib("libc.so.6")

	resist_func("libc.so.6", "malloc", "p", "u")
	resist_func("libc.so.6", "free", "v", "p")
	resist_func("libc.so.6", "memset", "p", "piu")
	resist_func("libc.so.6", "write", "u", "ipu")

	len = length("hello\n")
	mem = malloc(len)
	
	memset(mem    , 0x68, 1)  # 'h'
	memset(mem + 1, 0x65, 1)  # 'e'
	memset(mem + 2, 0x6c, 2)  # 'll'
	memset(mem + 4, 0x6f, 1)  # 'o'
	memset(mem + 5, 0x0a, 1)  # '\n'

	write(1, mem, len)

	free(mem)
}
