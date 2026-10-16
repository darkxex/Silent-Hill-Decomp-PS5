#include <stdio.h>

int sceSystemServiceLoadExec(const char* path, const char** arguments);
int sceKernelUsleep(unsigned microseconds);

/* exit() raises SIGSYS on the console; asking the system to run "exit" is what ends a title normally. */
void __wrap_exit(int code)
{
    (void)code;
    fflush(NULL);
    for (;;)
    {
        sceSystemServiceLoadExec("exit", NULL);
        sceKernelUsleep(1000000);
    }
}

void __wrap__Exit(int code)
{
    __wrap_exit(code);
}
