-------------
Interposition
-------------

From:

driver <--> function

To:

driver <--> interposed function <--> function

For replacing system calls there is a hack based on modifying the cr0 register:

linux 3.14 sys_call_table interception module
https://bbs.archlinux.org/viewtopic.php?id=139406

(check out bit 16)
http://en.wikipedia.org/wiki/Control_register


Example
-------

1) actual int Double(int) function defined in double.c
2) main driver calling an extern int Double(int) function defined in doublemain.c
3) interposed function: which replaces Double: does something then invokes the actual Double function;
   defined in doublewrap.c

Two main options:
* dlsym(RTLD_NEXT,...)
* ld --wrap=symbol (see ld man page entry at the end)

Linux
-----

Implement wrapper Double function in doublewrap.c as:

#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>

static int (*RealDouble)(int) = NULL;

int MyDouble(int i) {
	printf("Double %d - called\n", i);
    if(RealDouble == NULL) {
        RealDouble = (int (*)(int)) dlsym(RTLD_NEXT, "Double");
        assert(RealDouble != NULL);
    }
    return RealDouble(i);
}

1) Compile double.c and doublewrap.c as shared objects: libdouble.so, libdoublewrapper.so
2) Compile driver linking it to shared object libdouble.so
3) invoke driver as: LD_PRELOAD=./libdoublewrapper.so doublemain


Mac OS
------

Use MacOS/clang specific flags:

clang -dynamiclib ../doublewrap.c -o libdoublewrapper.dylib -L. -ldouble

Standard RTLD_NEXT approach works for system libraries (to e.g. wrap printf), but invocation
has to be:

DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES=./libdoublewrapper.dylib ./a.out

For user generated libraries I am using the DYLD_INTERPOSE solution:

1) Compile double.c as dylib shared object: libdouble.dylib, libdoublewrapper.dylib
2) Compile driver linking to libdouble.dylib
3) Write in doublewrap.c a wrapper function with a name different than the wrapped
   function and use the DYLD_INTERPOSE macro to perform the symbol substitution
   at load time (see below);
4) Compile doublewrap.c to libdoublewrapper.dylib and link with libdouble.dylib

Launch as DYLD_INSERT_LIBRARIES=./libdoublewrapper.dylib ...

Code:

#include <stdio.h>
#include <assert.h>

extern int Double(int);

int MyDouble(int i) {
	printf("Double %d - called\n", i);
    return Double(i);
}

#define DYLD_INTERPOSE(_replacement,_replacee) \
   __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

DYLD_INTERPOSE(MyDouble, Double)       


ld --wrap
---------

 --wrap=symbol
           Use a wrapper function for symbol.  Any undefined reference to
           symbol will be resolved to "__wrap_symbol".  Any undefined
           reference to "__real_symbol" will be resolved to symbol.

           This can be used to provide a wrapper for a system function.  The
           wrapper function should be called "__wrap_symbol".  If it wishes
           to call the system function, it should call "__real_symbol".

           Here is a trivial example:

                   void *
                   __wrap_malloc (size_t c)
                   {
                     printf ("malloc called with %zu\n", c);
                     return __real_malloc (c);
                   }

           If you link other code with this file using --wrap malloc, then
           all calls to "malloc" will call the function "__wrap_malloc"
           instead.  The call to "__real_malloc" in "__wrap_malloc" will
           call the real "malloc" function.

           You may wish to provide a "__real_malloc" function as well, so
           that links without the --wrap option will succeed.  If you do
           this, you should not put the definition of "__real_malloc" in the
           same file as "__wrap_malloc"; if you do, the assembler may
           resolve the call before the linker has a chance to wrap it to
           "malloc".










