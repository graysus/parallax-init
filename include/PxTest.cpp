
#include <PxIPC.hpp>

#ifndef PXTEST
#define PXTEST

std::string PxTestMain(int argc, const char *argv[]);

int main(int argc, const char *argv[]) {
	auto s = PxTestMain(argc, argv);
	s = PxFunction::join(PxFunction::split(s, "\\"), "\\\\");
	s = PxFunction::join(PxFunction::split(s, "\n"), "\\n");
	PxIPC::Client cli;
	cli.Connect("/tmp/pxtest.sock");
	cli.Write((std::string)"report "+argv[1]+" "+s+'\0');
}

#endif
