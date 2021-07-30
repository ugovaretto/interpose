//Mac OS X only, @TODO add conditional compilation for Mac OS and Linux
//Uncomment commented code and comment DYLD_* macros
//#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>

static int (*RealDouble)(int) = NULL;

extern int Double(int);

int MyDouble(int i) {
	printf("Double %d - called\n", i);
    // if(RealDouble == NULL) {
    // 	//assert(dlsym(RTLD_NEXT, "printf"));
    //<-- THIS IS WHAT REPLACES THE ACTUAL FUNCTION
    //     RealDouble = (int (*)(int)) dlsym(RTLD_NEXT, "Double"); 
    //-->
    //     assert(RealDouble != NULL);
    // }
    // return RealDouble(i);
    return Double(i);
}

#define DYLD_INTERPOSE(_replacement,_replacee) \
   __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

DYLD_INTERPOSE(MyDouble, Double)            
