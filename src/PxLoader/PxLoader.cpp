#include <unistd.h>
#include <cstdio>

int main(int argc, char *const argv[]) {
	if (execv("/usr/lib/parallax/PxInit", argv) < 0) {
		perror("failed to start PxInit: execv");
	}
	return 1;
}
