#include <PxArg.hpp>
#include <PxTest.cpp>
#include <PxFunction.hpp>

std::string PxTestMain(int argc, const char *argv[]) {
	PxArg::strvec v = PxFunction::vectorize(argc, argv);

	PxArg::Argument arg1("argument-1", 'a');
	PxArg::Argument arg2("argument-2", 'b');
	PxArg::Argument arg3("argument-3", 'c');
	PxArg::Argument arglong("long-argument");
	PxArg::Argument arglong2("long-argument-2");
	PxArg::ValueArgument argvalx("argument-x", 'x');
	PxArg::ValueArgument argvaly("argument-y", 'y');
	PxArg::ValueArgument argvalz("argument-z", 'z');

	auto res = PxArg::parseArgs(v, {&arg1, &arg2, &arg3, &arglong, &arglong2, &argvalx, &argvaly, &argvalz});

	std::string report;
	if (arg1.active) report += "A1;";
	if (arg2.active) report += "A2;";
	if (arg3.active) report += "A3;";
	if (arglong.active) report += "AL;";
	if (arglong2.active) report += "AL2;";
	if (argvalx.active) report += "AVX;"+argvalx.value+";";
	if (argvaly.active) report += "AVY;"+argvaly.value+";";
	if (argvalz.active) report += "AVZ;"+argvalz.value+";";

	return report;
}
