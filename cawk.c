#include <dlfcn.h>
#include <ffi.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//-------------
#include <sys/stat.h>
//-------------

#include "gawkapi.h"

#include "gettext.h"
#define _(msgid)  gettext(msgid)
#define N_(msgid) msgid

//-------------
#define AWKNUM	double
//# define EXIT_FATAL   2
//-------------

int plugin_is_GPL_compatible;

#if ARCH_X86
	#define INS_SIZE 16
#elif ARCH_X64
	#define INS_SIZE 32
#endif

#define cawk_get_argument(count, wanted, result)	\
	if (!get_argument(count, wanted, result)) {\
		\
	}

enum cawk_type {
	CAWK_VOID,
	CAWK_SINT,
	CAWK_UINT,
	CAWK_FLOAT,
	CAWK_DOUBLE,
	CAWK_STRING,
	CAWK_POINTER,
};

static int pagesize;

struct shlib_t {
	char *lib_name;
	void *lib_ptr;
};

static struct shlib_t shlib[16];

struct ffi_func_t {
	ffi_cif cif;
	void *func_ptr;
	enum cawk_type func_cawk_type;
	ffi_type *func_ffi_type;
	int arg_num;
	enum cawk_type *arg_cawk_types;
	ffi_type **arg_ffi_type;
	void **arg_values;
};

//static struct ffi_func_t *ffi_func_args[512];

static void *exec_page;

static unsigned int shlib_number;

static unsigned int func_number;
static struct ffi_func_t *call_ffi_func;

static awk_value_t *do_pseudo(int nargs, awk_value_t *result);
static void set_trampoline(struct ffi_func_t *ffi_func_ptr);
static void *lookup_shlib(const char *name);
static awk_value_t *do_resist_func(int nargs, awk_value_t *result);
static awk_value_t *do_load_shlib(int nargs, awk_value_t *result);
static awk_value_t *do_close_shlib(int nargs, awk_value_t *result);
static awk_bool_t init_cawk(void);

static const gawk_api_t *api;	/* for convenience macros to work */
static awk_ext_id_t *ext_id;
static const char *ext_version = "cawk: version 0.1";
static awk_bool_t (*init_func)(void) = init_cawk;

static awk_value_t *
do_load_shlib(int nargs, awk_value_t *result)
{
	awk_value_t obj;
	void *dl;
	int flags;

	if (do_lint && nargs != 1)
		lintwarn(ext_id, _("load_shlib: called with incorrect number of arguments, expecting 1"));

	if (!get_argument(0, AWK_STRING, &obj)) {
		// TODO
	}

	flags = RTLD_LAZY;
#ifdef RTLD_GLOBAL
	flags |= RTLD_GLOBAL;
#endif
	if ((dl = dlopen(obj.str_value.str, flags)) == NULL) {
		/* fatal needs `obj', and we need to deallocate it! */
		fatal(ext_id, _("fatal: extension: cannot open `%s' (%s)\n"),
			obj.str_value.str, dlerror());
		//gawk_exit(EXIT_FATAL);
	}

	emalloc(shlib[shlib_number].lib_name, char *, obj.str_value.len + 1, "load_shlib");
	strncpy(shlib[shlib_number].lib_name, obj.str_value.str, obj.str_value.len + 1);

	shlib[shlib_number].lib_ptr = dl;

	shlib_number++;

	return make_number((AWKNUM) (unsigned int) dl, result);
}

static awk_value_t *
do_close_shlib(int nargs, awk_value_t *result)
{
	awk_value_t lib;
	void *dl;
	int ret;

	if (do_lint && nargs != 1)
		lintwarn(ext_id, _("close_shlib: called with incorrect number of arguments, expecting 1"));

	if (!get_argument(0, AWK_STRING, &lib)) {
		// TODO
	}

	dl = lookup_shlib(lib.str_value.str);

	if ((ret = dlclose(dl)) != 0) {
		/* fatal needs `obj', and we need to deallocate it! */
		fatal(ext_id, _("fatal: extension: cannot close `%s' (%s)\n"),
			lib.str_value.str, dlerror());
		//gawk_exit(EXIT_FATAL);
	}

	return make_number((AWKNUM) ret, result);
}

static void
set_trampoline(struct ffi_func_t *ffi_func_ptr)
{
	char ins[INS_SIZE];
	signed int addr_diff;
	int i;

	i = 0;

#if ARCH_X86 || ARCH_X64
	// mov imm to func_number
#if ARCH_X64
	ins[i++] = 0x48;	// REX Prefix
#endif
	ins[i++] = 0xc7;	// x86/x64 mov instruction
	ins[i++] = 0x05;	// ModR/M Byte
#if ARCH_X86
	ins[i++] = (char)(((unsigned long) &call_ffi_func      ) & 0xff);
	ins[i++] = (char)(((unsigned long) &call_ffi_func >>  8) & 0xff);
	ins[i++] = (char)(((unsigned long) &call_ffi_func >> 16) & 0xff);
	ins[i++] = (char)(((unsigned long) &call_ffi_func >> 24) & 0xff);
#elif ARCH_X64
	#define LENGTH_OF_MOV_INSTRUCTION	11
	addr_diff = (void*)&call_ffi_func - (void*)(exec_page
			+ INS_SIZE * func_number + i + LENGTH_OF_MOV_INSTRUCTION);
	ins[i++] = (char)(((signed long) addr_diff      ) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >>  8) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >> 16) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >> 24) & 0xff);
#endif
	ins[i++] = (char)(((unsigned int) ffi_func_ptr      ) & 0xff);
	ins[i++] = (char)(((unsigned int) ffi_func_ptr >>  8) & 0xff);
	ins[i++] = (char)(((unsigned int) ffi_func_ptr >> 16) & 0xff);
	ins[i++] = (char)(((unsigned int) ffi_func_ptr >> 24) & 0xff);
#if ARCH_X64
//?++
	ins[i++] = 0x48;	// REX Prefix
	ins[i++] = 0xc7;	// x86/x64 mov instruction
	ins[i++] = 0x05;	// ModR/M Byte

	addr_diff = (void*)&call_ffi_func - (void*)(exec_page
			+ INS_SIZE * func_number + i + LENGTH_OF_MOV_INSTRUCTION) - 2;
	ins[i++] = (char)(((signed long) addr_diff      ) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >>  8) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >> 16) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >> 24) & 0xff);

	ins[i++] = (char)(((unsigned int) ffi_func_ptr >> 32) & 0xff);
	ins[i++] = (char)(((unsigned int) ffi_func_ptr >> 40) & 0xff);
	ins[i++] = (char)(((unsigned int) ffi_func_ptr >> 48) & 0xff);
	ins[i++] = (char)(((unsigned int) ffi_func_ptr >> 56) & 0xff);
//?--
#endif

	// jmp to do_pseudo
	// x86/x64 jmp instruction
	//	E9 cd JMP rel32
	#define LENGTH_OF_JMP_INSTRUCTION	5
	addr_diff = (void*)do_pseudo - (void*)(exec_page
			+ INS_SIZE * func_number + i + LENGTH_OF_JMP_INSTRUCTION);
	ins[i++] = 0xe9;	// x86/x64 jmp instruction
	ins[i++] = (char)(((signed long) addr_diff      ) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >>  8) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >> 16) & 0xff);
	ins[i++] = (char)(((signed long) addr_diff >> 24) & 0xff);
//printf("##%d\n",i);
	/* fill as 'NOP' instruction */
	for ( ; i < INS_SIZE; i++)
		ins[i] = 0x90;
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
			// do_pseudo,
			exec_page,
			dlload,
			addr_diff
	      );
#endif
}

static void *
lookup_shlib(const char *name)
{
	int i;

	for (i = 0; i < shlib_number; i++)
		if (!strcmp(name, shlib[i].lib_name))
			return shlib[i].lib_ptr;

	return NULL;
}

static awk_value_t *
do_resist_func(int nargs, awk_value_t *result)
{
	ffi_status status;
	awk_value_t lib;
	awk_value_t fun;
	awk_value_t ret;
	awk_value_t arg;
	int i;
	void *dl;
	void *func;
        ffi_cif cif;
	int arg_num;
	enum cawk_type *arg_cawk_types;
	ffi_type **arg_ffi_type;
	void **arg_values;
	enum cawk_type func_cawk_type;
	ffi_type *func_ffi_type;
	struct ffi_func_t *ffi_func;
	awk_ext_func_t ext_func;

	if (do_lint && nargs != 1)
		lintwarn(ext_id, _("resit_func: called with incorrect number of arguments, expecting 4"));

	if (!get_argument(0, AWK_STRING, &lib)) {
		// TODO
	}

	if (!get_argument(1, AWK_STRING, &fun)) {
		// TODO
	}

	if (!get_argument(2, AWK_STRING, &ret)) {
		// TODO
	}

	if (!get_argument(3, AWK_STRING, &arg)) {
		// TODO
	}

	dl = lookup_shlib(lib.str_value.str);

	if ((func = dlsym(dl, fun.str_value.str)) == NULL) {
		fatal(ext_id, _(
		    "fatal: extension: library `%s': cannot call function `%s' (%s)\n"),
				"obj->stptr", fun.str_value.str, dlerror());
		//gawk_exit(EXIT_FATAL);
		return NULL; /* surppress warning */
	}

	switch (ret.str_value.str[0]) {
	case 'v':
		func_cawk_type = CAWK_VOID;
		func_ffi_type = &ffi_type_void;
		break;
	case 'i':
	case 's':
		func_cawk_type = CAWK_SINT;
		func_ffi_type = &ffi_type_sint;
		break;
	case 'u':
		func_cawk_type = CAWK_UINT;
		func_ffi_type = &ffi_type_uint;
		break;
	case 'f':
		func_cawk_type = CAWK_FLOAT;
		func_ffi_type = &ffi_type_float;
		break;
	case 'd':
		func_cawk_type = CAWK_DOUBLE;
		func_ffi_type = &ffi_type_double;
		break;
	case '$':
		func_cawk_type = CAWK_STRING;
		func_ffi_type = &ffi_type_pointer;
		break;
	case 'p':
		func_cawk_type = CAWK_POINTER;
		func_ffi_type = &ffi_type_pointer;
		break;
	default:
		func_ffi_type = NULL; /* surppress warning */
		break;
	}

	arg_num = strlen(arg.str_value.str);

	emalloc(arg_cawk_types, enum cawk_type *, arg_num * sizeof(enum cawk_type), "resist_func");
	emalloc(arg_ffi_type, ffi_type **, arg_num * sizeof(ffi_type *), "resist_func");
	emalloc(arg_values, void **, arg_num * sizeof(void *), "resist_func");

	for (i = 0; i < arg_num; i++) {
		//printf ("%c\n",arg.str_value.str[i + 1]);
		switch (arg.str_value.str[i]) {
		case 'v':
			arg_cawk_types[i] = CAWK_VOID;
			arg_ffi_type[i] = &ffi_type_void;
			arg_values[i] = NULL;
			if (arg_num != 1) /* Error TODO */;
			arg_num = 0;
			break;
		case 'i':
		case 's':
			arg_cawk_types[i] = CAWK_SINT;
			arg_ffi_type[i] = &ffi_type_sint;
			emalloc(arg_values[i], int *, sizeof(int), "resist_func");
			break;
		case 'u':
			arg_cawk_types[i] = CAWK_UINT;
			arg_ffi_type[i] = &ffi_type_uint;
			emalloc(arg_values[i], unsigned int *, sizeof(unsigned int), "resist_func");
			break;
		case 'f':
			arg_cawk_types[i] = CAWK_FLOAT;
			arg_ffi_type[i] = &ffi_type_float;
			emalloc(arg_values[i], float *, sizeof(float), "resist_func");
			break;
		case 'd':
			arg_cawk_types[i] = CAWK_DOUBLE;
			arg_ffi_type[i] = &ffi_type_double;
			emalloc(arg_values[i], double *, sizeof(double), "resist_func");
			break;
		case '$':
			arg_cawk_types[i] = CAWK_STRING;
			arg_ffi_type[i] = &ffi_type_pointer;
			emalloc(arg_values[i], char **, sizeof(char *), "resist_func");
			break;
		case 'p':
			arg_cawk_types[i] = CAWK_POINTER;
			arg_ffi_type[i] = &ffi_type_pointer;
			emalloc(arg_values[i], void **, sizeof(void *), "resist_func");
			break;
		default:
			break;
		}
	}

        if ((status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI,
				arg_num, func_ffi_type, arg_ffi_type)) != FFI_OK) {
                // Handle the ffi_status error. TODO
        }

	ffi_func = malloc(sizeof(struct ffi_func_t));

	ffi_func->func_ptr = func;
	ffi_func->cif = cif;
	ffi_func->func_cawk_type = func_cawk_type;
	ffi_func->func_ffi_type = func_ffi_type;
	ffi_func->arg_num = arg_num;
	ffi_func->arg_cawk_types = arg_cawk_types;
	ffi_func->arg_ffi_type = arg_ffi_type;
	ffi_func->arg_values = arg_values;

	set_trampoline(ffi_func);

	ext_func.name = fun.str_value.str;
	ext_func.function = (awk_value_t *(*)(int, awk_value_t*)) (exec_page + func_number * INS_SIZE);
	ext_func.num_expected_args = arg_num;
	add_ext_func(NULL, &ext_func);

	func_number++;

	return make_number((AWKNUM) 0, result);
}

static awk_value_t *
do_pseudo(int nargs, awk_value_t *result)
{
	union func_result {
		ffi_sarg sint;
		ffi_arg uint;
		float flt;
		double dbl;
		void *ptr;
	};

	//unsigned int fnum;
	struct ffi_func_t *ffi_func;
	//void **arg_values;
	union func_result ret_val;
	int i;
	
	//fnum = call_ffi_func;
	//ffi_func = ffi_func_args[fnum];
	ffi_func = call_ffi_func;
	//arg_values = ffi_func->arg_values;

	// printf("%016lx\n", fnum);

	for (i = 0; i < ffi_func->arg_num; i++) {
		awk_value_t arg;

		switch (ffi_func->arg_cawk_types[i]) {
		case CAWK_VOID:
			break;
		case CAWK_SINT:
			cawk_get_argument(i, AWK_NUMBER, &arg);
			*(int *) ffi_func->arg_values[i] =
					(int) arg.num_value;
			break;
		case CAWK_UINT:
			cawk_get_argument(i, AWK_NUMBER, &arg);
			*(unsigned int *) ffi_func->arg_values[i] =
					(unsigned int) arg.num_value;
			break;
		case CAWK_FLOAT:
			cawk_get_argument(i, AWK_NUMBER, &arg);
			*(float *) ffi_func->arg_values[i] =
					(float) arg.num_value;
			break;
		case CAWK_DOUBLE:
			cawk_get_argument(i, AWK_NUMBER, &arg);
			*(double *) ffi_func->arg_values[i] =
					(double) arg.num_value;
			break;
		case CAWK_STRING:
			cawk_get_argument(i, AWK_STRING, &arg);
			*(char **)ffi_func->arg_values[i] =
					(char *) arg.str_value.str;
			break;
		case CAWK_POINTER:
			cawk_get_argument(i, AWK_NUMBER, &arg);
			*(void **)ffi_func->arg_values[i] =
					(void *) (unsigned int) arg.num_value;
			break;
                }
	}

	// Invoke the function.
	ffi_call(&ffi_func->cif, FFI_FN(ffi_func->func_ptr),
		&ret_val, ffi_func->arg_values);

	switch (ffi_func->func_cawk_type) {
	case CAWK_VOID:
		return make_null_string(result);
	case CAWK_SINT:
		return make_number((AWKNUM) ret_val.sint, result);
	case CAWK_UINT:
		return make_number((AWKNUM) ret_val.uint, result);
	case CAWK_FLOAT:
		return make_number((AWKNUM) ret_val.flt, result);
	case CAWK_DOUBLE:
		return make_number((AWKNUM) ret_val.dbl, result);
	case CAWK_STRING:
		return make_const_string(ret_val.ptr, strlen(ret_val.ptr), result);
	case CAWK_POINTER:
		return make_number((AWKNUM) (unsigned int) ret_val.ptr, result);
	}

	return make_null_string(result);
}

/* init_cawk --- initialization routine */
static awk_bool_t
init_cawk(void)
{
	pagesize = sysconf(_SC_PAGE_SIZE);
	exec_page = mmap(NULL, pagesize * 1, PROT_WRITE | PROT_EXEC,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	shlib_number = 0;
	func_number = 0;

	return awk_true;
}

static awk_ext_func_t func_table[] = {
	{ "load_shlib", do_load_shlib, 1 },
	{ "close_shlib", do_close_shlib, 1 },
	{ "resist_func", do_resist_func, 4 },
};

/* define the dl_load function using the boilerplate macro */
dl_load_func(func_table, cawk, "")
