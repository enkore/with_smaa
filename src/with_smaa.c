
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv)
{
    if(argc < 2) {
	fprintf(stderr, "with_smaa: no program specified\n");
	return 1;
    }

    setenv("LD_PRELOAD", "libwith_smaa_shim.so", 1);

    execvp(argv[1], &argv[1]);

    unsetenv("LD_PRELOAD");

    return errno;
}
