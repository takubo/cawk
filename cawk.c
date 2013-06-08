#include <sys/mman.h>
#include <malloc.h>

#include <dlfcn.h>
#include <ffi.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "awk.h"

int plugin_is_GPL_compatible;

void *DL;
void *CAWK_FUNC;
void *exec_page;

unsigned int func_number;

static NODE*
do_tmp(int nargs)
{
	ffi_cif cif;
	ffi_type *arg_types[1];
	void *arg_values[2];
	ffi_arg result;
	ffi_status status;
	
	arg_types[0] = &ffi_type_void;

printf("%016lx\n", func_number);

	// Prepare the ffi_cif structure.
	if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
			0, &ffi_type_void, arg_types)) != FFI_OK)
	{
		// Handle the ffi_status error.
	}

	//arg_values[0] = NULL;
	// hello();

	// Invoke the function.
	ffi_call(&cif, FFI_FN(CAWK_FUNC), &result, arg_values);

	return make_number((AWKNUM) 0);
}

static NODE *
do_load_shlib(int nargs)
{
	NODE *obj;
	void *dl;
	int flags = RTLD_LAZY;
	int fatal_error = FALSE;

	obj = (NODE *) get_scalar_argument(0, FALSE);
	force_string(obj);

#ifdef RTLD_GLOBAL
	flags |= RTLD_GLOBAL;
#endif
	if ((dl = dlopen(obj->stptr, flags)) == NULL) {
		/* fatal needs `obj', and we need to deallocate it! */
		msg(_("fatal: extension: cannot open `%s' (%s)\n"), obj->stptr,
		      dlerror());
		fatal_error = TRUE;
		goto done;
	}
	DL = dl;

done:
	if (fatal_error)
		gawk_exit(EXIT_FATAL);
	return Nnull_string; 
}

static NODE*
do_c_func_resist(int nargs)
{
	NODE *lib;
	NODE *fun;
	NODE *arg;
	int arg_num;
	void *func;
	int fatal_error = FALSE;

	lib = (NODE *) get_scalar_argument(0, FALSE);
	force_string(lib);

	fun = (NODE *) get_scalar_argument(1, FALSE);
	force_string(fun);

	arg = (NODE *) get_scalar_argument(2, FALSE);
	force_string(arg);

	func = dlsym(DL, fun->stptr);
	if (func == NULL) {
		msg(_("fatal: extension: library `%s': cannot call function `%s' (%s)\n"),
				"obj->stptr", fun->stptr, dlerror());
		fatal_error = TRUE;
		goto done;
	}
	CAWK_FUNC = func;

	arg_num = strlen(arg->stptr);

done:
	if (fatal_error) {
		gawk_exit(EXIT_FATAL);
		return NULL; /* surpress warning */
	}

	// make_builtin(fun->stptr, do_tmp, arg_num);
	make_builtin(fun->stptr, (NODE*(*)(int))(exec_page + 0), arg_num);

	return make_number((AWKNUM) 0);
}

NODE *
dlload(NODE *tree, void *dl)
{
	make_builtin("load_shlib", do_load_shlib, 1);
	make_builtin("c_func_resist", do_c_func_resist, 3);
	// make_builtin("c_call", do_c_call, 32);
	// make_builtin("c_call_type", do_c_call_type, 33);


	int pagesize;
	pagesize = sysconf(_SC_PAGE_SIZE);
	// printf("%d\n",pagesize);

//	mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
	exec_page = mmap(NULL, pagesize * 1, PROT_WRITE | PROT_EXEC,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

// munmap

//---------------------------------------------
//printf("%d\n", exec_page);
//unsigned long lo =  0xc3c3c3c3;
//memset(exec_page, 0xc3, 256);
//*((long*)exec_page) = 0xc3c3c3c3;
//((void (*)())exec_page)();
//---------------------------------------------

//---------------------------------------------
//*((unsigned char*)exec_page+3) = 0xea;
//*((unsigned char*)exec_page+2) = 0x00;
//*((unsigned char*)exec_page+1) = (char)(((unsigned int)do_tmp >>  0) & 0xff);
//*((unsigned char*)exec_page+0) = (char)(((unsigned int)do_tmp >>  8) & 0xff);
//*((unsigned char*)exec_page+3) = (char)(((unsigned int)do_tmp >> 16) & 0xff);
//*((unsigned char*)exec_page+4) = (char)(((unsigned int)do_tmp >> 24) & 0xff);
//---------------------------------------------

//---------------------------------------------
//ins[0] = 0xea;
//ins[1] = (char)(((unsigned int)do_tmp >> 24) & 0xff);
//ins[2] = (char)(((unsigned int)do_tmp >> 16) & 0xff);
//ins[3] = (char)(((unsigned int)do_tmp >>  8) & 0xff);
//ins[4] = (char)(((unsigned int)do_tmp      ) & 0xff);
//memcpy(exec_page, ins, 5);
//---------------------------------------------

//---------------------------------------------
func_number = 0x00003068;
printf("%016lx\n", func_number);
#if ARCH_X86
char ins[16];
#elif ARCH_X64
char ins[32];
#endif
signed int addr_diff; // 5 is ins next of jmp
int i = 0;

//// mov imm to func_number
#if ARCH_X64
#define LENGTH_OF_MOV_INSTRUCTION	11
addr_diff = (void*)&func_number - (void*)(exec_page + (sizeof(ins) / sizeof(ins[0])) * 0 + i + LENGTH_OF_MOV_INSTRUCTION);
// printf("%016p %016p %016p \n", (void *)addr_diff , &func_number , (void*)(exec_page + (sizeof(ins) / sizeof(ins[0])) * 0 + i + 10));

ins[i++] = 0x48;	// REX Prefix
#endif
//#if ARCH_X86 || ARCH_X64
ins[i++] = 0xc7;	// x86 mov instruction
ins[i++] = 0x05;	// ModR/M Byte
//#endif
#if ARCH_X86
ins[i++] = (char)(((unsigned long)&func_number      ) & 0xff);
ins[i++] = (char)(((unsigned long)&func_number >>  8) & 0xff);
ins[i++] = (char)(((unsigned long)&func_number >> 16) & 0xff);
ins[i++] = (char)(((unsigned long)&func_number >> 24) & 0xff);
#elif ARCH_X64
ins[i++] = (char)(((signed long)addr_diff      ) & 0xff);
ins[i++] = (char)(((signed long)addr_diff >>  8) & 0xff);
ins[i++] = (char)(((signed long)addr_diff >> 16) & 0xff);
ins[i++] = (char)(((signed long)addr_diff >> 24) & 0xff);
#endif
// #if ARCH_X64
// ins[i++] = (char)(((unsigned long)&func_number >> 32) & 0xff);
// ins[i++] = (char)(((unsigned long)&func_number >> 40) & 0xff);
// ins[i++] = (char)(((unsigned long)&func_number >> 48) & 0xff);
// ins[i++] = (char)(((unsigned long)&func_number >> 56) & 0xff);
// #endif
ins[i++] = (char)(((unsigned int)func_number      ) & 0xff);
ins[i++] = (char)(((unsigned int)func_number >>  8) & 0xff);
ins[i++] = (char)(((unsigned int)func_number >> 16) & 0xff);
ins[i++] = (char)(((unsigned int)func_number >> 24) & 0xff);
// #if ARCH_X64
// ins[i++] = (char)(((unsigned long)func_number >> 32) & 0xff);
// ins[i++] = (char)(((unsigned long)func_number >> 40) & 0xff);
// ins[i++] = (char)(((unsigned long)func_number >> 48) & 0xff);
// ins[i++] = (char)(((unsigned long)func_number >> 56) & 0xff);
// #endif


// jmp to do_tmp
// x86 jmp instruction
// E9 cw JMP rel16 æ¬¡ã®å½ä»¤ã¨ã®ç¸å¯¾åéåã ãç¸å¯¾ near ã¸ã£ã³ãããã
// E9 cd JMP rel32 æ¬¡ã®å½ä»¤ã¨ã®ç¸å¯¾åéåã ãç¸å¯¾ near ã¸ã£ã³ãããã
#define LENGTH_OF_JMP_INSTRUCTION	5
addr_diff = (void*)do_tmp - (void*)(exec_page + (sizeof(ins) / sizeof(ins[0])) * 0 + i + LENGTH_OF_JMP_INSTRUCTION); // 5 is ins next of jmp
ins[i++] = 0xe9;	// x86 jmp instruction
ins[i++] = (char)(((signed long)addr_diff      ) & 0xff);
ins[i++] = (char)(((signed long)addr_diff >>  8) & 0xff);
ins[i++] = (char)(((signed long)addr_diff >> 16) & 0xff);
ins[i++] = (char)(((signed long)addr_diff >> 24) & 0xff);

// fill by nop
for ( ; i < (sizeof(ins) / sizeof(ins[0])); i++) {
	ins[i] = 0x90;
}

memcpy(exec_page, ins, (sizeof(ins) / sizeof(ins[0])));
//---------------------------------------------

#if 0
 printf("%d\n",i);
 printf("%02x %02x %02x %02x %02x %p %x %x %x\n",
 *((unsigned char*)exec_page+0),
 *((unsigned char*)exec_page+1),
 *((unsigned char*)exec_page+2),
 *((unsigned char*)exec_page+3),
 *((unsigned char*)exec_page+4),
 &func_number,
 // do_tmp,
 exec_page,
 dlload,
 addr_diff
 );
#endif

	return make_number((AWKNUM) 0);
}

#if 0
static NODE*
do_c_fllunc_resist(int nargs)
{
	NODE *fun;
	char cunc_name;

	NODE *(*func)(NODE *, void *);
	NODE *tmp = NULL;
	fun = POP_STRING();

	func = (NODE *(*)(NODE *, void *)) dlsym(dl, fun->stptr);
	if (func == NULL) {
		msg(_("fatal: extension: library `%s': cannot call function `%s' (%s)\n"),
				obj->stptr, fun->stptr, dlerror());
		fatal_error = TRUE;
		goto done;
	}

	tmp = (*func)(obj, dl);
	if (tmp == NULL)
		tmp = Nnull_string;

	DEREF(fun);

	make_builtin(func_name, do_tmp, arg_num);
	return make_number((AWKNUM) 0);
}

#endif
