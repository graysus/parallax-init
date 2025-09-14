#include <PxIPC.hpp>
#include <PxResult.hpp>
#include <PxFunction.hpp>
#include <PxLog.hpp>
#include <iterator>
#include <string>

int main(int argc, const char* argv[]) {
	auto args = PxFunction::vectorize(argc, argv);
	std::string bindTo;
	std::string toWrite;
	if (PxFunction::endsWith(args[0], "/sendmsgin") || args[0] == "sendmsgin") {
		bindTo = "/run/parallax-pid1.sock";
	} else if (PxFunction::endsWith(args[0], "/sendmsgu") || args[0] == "sendmsgu") {
		bindTo = "/run/parallax-svman-u"+std::to_string(getuid())+".sock";
	} else {
		bindTo = "/run/parallax-svman.sock";
	}
	
	std::vector<std::string> args2(std::next(args.begin()), args.end());

	PxIPC::Client cli;

	cli.Connect(bindTo).assert("main");
	cli.Write(PxFunction::join(args2, " ")+'\0').assert("main");
}
