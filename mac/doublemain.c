#include <stdio.h>

extern int Double(int);

int main(int argc, char** argv) {
    printf("Main: %d\n", Double(3));	
	return 0;
}