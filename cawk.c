#include <sys/mman.h>
#include <malloc.h>

#include <dlfcn.h>
#include <ffi.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "awk.h"

#if ARCH_X86
	#define INS_SIZE 16
#elif ARCH_X64
	#define INS_SIZE 32
#endif

int plugin_is_GPL_compatible;

struct shlib_t {
	char lib_name[64];
	void *lib_ptr;
};

struct shlib_t shlib[16];
	
struct ffi_func_args_t {
	ffi_cif cif;
	void *func_ptr;
	ffi_type *func_type;
	int arg_num;
	ffi_type **arg_types;
	void **arg_values;
};

struct ffi_func_args_t ffi_func_args[512];


void *exec_page;

unsigned int shlib_number;

unsigned int func_number;
unsigned int call_func_number;

union func_result {
	ffi_sarg sint;
	ffi_arg uint;
	float flt;
	double dbl;
	void *ptr;
};

static NODE*
do_tmp(int nargs)
{
	unsigned int fnum = call_func_number;
	void **arg_values = ffi_func_args[fnum].arg_values;
	union func_result result;
	int i;
	
	// printf("%016lx\n", fnum);

	for (i = 0; i < ffi_func_args[fnum].arg_num; i++) {
		NODE *arg = (NODE *) get_scalar_argument(i, FALSE);

#if 1
		if (ffi_func_args[fnum].arg_types[i] == &ffi_type_void) {
		} else if (ffi_func_args[fnum].arg_types[i] == &ffi_type_sint) {
			*(int *)ffi_func_args[fnum].arg_values[i] = (int) force_number(arg);
		} else if (ffi_func_args[fnum].arg_types[i] == &ffi_type_uint) {
			*(unsigned int *)ffi_func_args[fnum].arg_values[i] = (unsigned int) force_number(arg);
		} else if (ffi_func_args[fnum].arg_types[i] == &ffi_type_float) {
			*(float *)ffi_func_args[fnum].arg_values[i] = (float) force_number(arg);
		} else if (ffi_func_args[fnum].arg_types[i] == &ffi_type_double) {
			*(double *)ffi_func_args[fnum].arg_values[i] = (double) force_number(arg);
		} else if (ffi_func_args[fnum].arg_types[i] == &ffi_type_pointer) {
			*(void **)ffi_func_args[fnum].arg_values[i] = (void *) (unsigned int) force_number(arg);
                }
#else
		switch (ffi_func_args[fnum].arg_types[i]) {
		case &ffi_type_void:
			break;
		case &ffi_type_sint:
			*arg_values[i] = (int) force_number(arg);
			break;
		case &ffi_type_uint:
			*arg_values[i] = (unsigned int) force_number(arg);
			break;
		case &ffi_type_float:
			*arg_values[i] = (float) force_number(arg);
			break;
		case &ffi_type_double:
			*arg_values[i] = (double) force_number(arg);
			break;
		case &ffi_type_pointer:
			*arg_values[i] = (void *) force_number(arg);
			break;
		default:
			break;
		}
#endif
	}

	// Invoke the function.
	ffi_call(&ffi_func_args[fnum].cif,
		FFI_FN(ffi_func_args[fnum].func_ptr),
		&result, arg_values);

	if (ffi_func_args[fnum].func_type == &ffi_type_void) {
		return Nnull_string;
	} else if (ffi_func_args[fnum].func_type == &ffi_type_sint) {
		return make_number((AWKNUM) result.sint);
	} else if (ffi_func_args[fnum].func_type == &ffi_type_uint) {
		return make_number((AWKNUM) result.uint);
	} else if (ffi_func_args[fnum].func_type == &ffi_type_float) {
		return make_number((AWKNUM) result.flt);
	} else if (ffi_func_args[fnum].func_type == &ffi_type_double) {
		return make_number((AWKNUM) result.dbl);
	} else if (ffi_func_args[fnum].func_type == &ffi_type_pointer) {
		return make_number((AWKNUM) (unsigned int)result.ptr);
	}

	return make_number((AWKNUM) 0);
}

void fff()
{
	char ins[INS_SIZE];
	signed int addr_diff;
	int i = 0;

#if ARCH_X86 || ARCH_X64
	// mov imm to func_number
#if ARCH_X64
	#define LENGTH_OF_MOV_INSTRUCTION	11
	addr_diff = (void*)&call_func_number -
		(void*)(exec_page + INS_SIZE * func_number + i + LENGTH_OF_MOV_INSTRUCTION);
	ins[i++] = 0x48;	// REX Prefix
#endif
	ins[i++] = 0xc7;	// x86 mov instruction
	ins[i++] = 0x05;	// ModR/M Byte
#if ARCH_X86
	ins[i++] = (char)(((unsigned long)&call_func_number      ) & 0xff);
	ins[i++] = (char)(((unsigned long)&call_func_number >>  8) & 0xff);
	ins[i++] = (char)(((unsigned long)&call_func_number >> 16) & 0xff);
	ins[i++] = (char)(((unsigned long)&call_func_number >> 24) & 0xff);
#elif ARCH_X64
	ins[i++] = (char)(((signed long)addr_diff      ) & 0xff);
	ins[i++] = (char)(((signed long)addr_diff >>  8) & 0xff);
	ins[i++] = (char)(((signed long)addr_diff >> 16) & 0xff);
	ins[i++] = (char)(((signed long)addr_diff >> 24) & 0xff);
#endif
	ins[i++] = (char)(((unsigned int)func_number      ) & 0xff);
	ins[i++] = (char)(((unsigned int)func_number >>  8) & 0xff);
	ins[i++] = (char)(((unsigned int)func_number >> 16) & 0xff);
	ins[i++] = (char)(((unsigned int)func_number >> 24) & 0xff);

	// jmp to do_tmp
	// x86 jmp instruction
	// E9 cw JMP rel16 æ¬¡ã®å½ä»¤ã¨ã®ç¸å¯¾åéåã ãç¸å¯¾ near ã¸ã£ã³ãããã
	// E9 cd JMP rel32 æ¬¡ã®å½ä»¤ã¨ã®ç¸å¯¾åéåã ãç¸å¯¾ near ã¸ã£ã³ãããã
	#define LENGTH_OF_JMP_INSTRUCTION	5
	addr_diff = (void*)do_tmp -
		(void*)(exec_page + INS_SIZE * func_number + i + LENGTH_OF_JMP_INSTRUCTION);
	ins[i++] = 0xe9;	// x86 jmp instruction
	ins[i++] = (char)(((signed long)addr_diff      ) & 0xff);
	ins[i++] = (char)(((signed long)addr_diff >>  8) & 0xff);
	ins[i++] = (char)(((signed long)addr_diff >> 16) & 0xff);
	ins[i++] = (char)(((signed long)addr_diff >> 24) & 0xff);

	// fill by nop
	for ( ; i < INS_SIZE; i++) {
		ins[i] = 0x90;
	}
#endif

	memcpy(exec_page + func_number * INS_SIZE, ins, INS_SIZE);

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
}

static void*
lookup_shlib(const char *name)
{
	int i;

	for (i = 0; i < 16; i++)
		if (!strcmp(name, shlib[i].lib_name))
			return shlib[i].lib_ptr;
	return NULL;
}

static NODE*
do_c_func_resist(int nargs)
{
	ffi_status status;
	NODE *lib;
	NODE *fun;
	NODE *arg;
	int i;
	void *dl;
	void *func;
        ffi_cif cif;
	int arg_num;
	ffi_type **arg_types;
	void **arg_values;
	ffi_type *func_type;

	lib = (NODE *) get_scalar_argument(0, FALSE);
	force_string(lib);

	fun = (NODE *) get_scalar_argument(1, FALSE);
	force_string(fun);

	arg = (NODE *) get_scalar_argument(2, FALSE);
	force_string(arg);

	dl = lookup_shlib(lib->stptr);

	func = dlsym(dl, fun->stptr);
	if (func == NULL) {
		msg(_("fatal: extension: library `%s': cannot call function `%s' (%s)\n"),
				"obj->stptr", fun->stptr, dlerror());
		gawk_exit(EXIT_FATAL);
		return NULL; /* surppress warning */
	}

	arg_num = strlen(arg->stptr) - 1;

	switch (arg->stptr[0]) {
	case 'v':
		func_type = &ffi_type_void;
		break;
	case 'i':
	case 's':
		func_type = &ffi_type_sint;
		break;
	case 'u':
		func_type = &ffi_type_uint;
		break;
	case 'f':
		func_type = &ffi_type_float;
		break;
	case 'd':
		func_type = &ffi_type_double;
		break;
	case 'p':
		func_type = &ffi_type_pointer;
		break;
	default:
		func_type = NULL; // surppress warning
		break;
	}

	arg_types = malloc(arg_num * sizeof(ffi_type *));
	arg_values = malloc(arg_num * sizeof(void *));

	for (i = 0; i < arg_num; i++) {
		//printf ("%c\n",arg->stptr[i + 1]);
		switch (arg->stptr[i + 1]) {
		case 'v':
			arg_types[i] = &ffi_type_void;
			arg_values[i] = NULL;
			break;
		case 'i':
		case 's':
			arg_types[i] = &ffi_type_sint;
			arg_values[i] = malloc(sizeof(int));
			break;
		case 'u':
			arg_types[i] = &ffi_type_uint;
			arg_values[i] = malloc(sizeof(unsigned int));
			break;
		case 'f':
			arg_types[i] = &ffi_type_float;
			arg_values[i] = malloc(sizeof(float));
			break;
		case 'd':
			arg_types[i] = &ffi_type_double;
			arg_values[i] = malloc(sizeof(double));
			break;
		case 'p':
			arg_types[i] = &ffi_type_pointer;
			arg_values[i] = malloc(sizeof(void *));
			break;
		default:
			break;
		}
	}

        if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
				arg_num, func_type, arg_types)) != FFI_OK)
        {
                // Handle the ffi_status error.
        }

	fff();

	make_builtin(fun->stptr, (NODE*(*)(int))(exec_page + func_number * INS_SIZE), arg_num);

	ffi_func_args[func_number].func_ptr = func;
	ffi_func_args[func_number].cif = cif;
	ffi_func_args[func_number].func_type = func_type;
	ffi_func_args[func_number].arg_num = arg_num;
	ffi_func_args[func_number].arg_types = arg_types;
	ffi_func_args[func_number].arg_values = arg_values;

	func_number++;

	return make_number((AWKNUM) 0);
}

static NODE *
do_load_shlib(int nargs)
{
	NODE *obj;
	void *dl;
	int flags = RTLD_LAZY;

	obj = (NODE *) get_scalar_argument(0, FALSE);
	force_string(obj);

#ifdef RTLD_GLOBAL
	flags |= RTLD_GLOBAL;
#endif
	if ((dl = dlopen(obj->stptr, flags)) == NULL) {
		/* fatal needs `obj', and we need to deallocate it! */
		msg(_("fatal: extension: cannot open `%s' (%s)\n"),
			obj->stptr, dlerror());
		gawk_exit(EXIT_FATAL);
	}

	strcpy(shlib[shlib_number].lib_name, obj->stptr);
	shlib[shlib_number].lib_ptr = dl;

	shlib_number++;

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

	exec_page = mmap(NULL, pagesize * 1, PROT_WRITE | PROT_EXEC,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// munmap

	shlib_number = 0;
	func_number = 0;

	return make_number((AWKNUM) 0);
}
