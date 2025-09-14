#include <PxArg.hpp>
#include <PxFunction.hpp>

int main(int argc, const char *argv[]) {
	PxArg::strvec v = PxFunction::vectorize(argc, argv);

	PxArg::Argument arg1("arg1", '1');
	PxArg::Argument arg2("arg2", '2');
	PxArg::Argument arg3("arg3", '3');
	PxArg::Argument arglong("arglong");
	PxArg::ValueArgument argval("argval", 'v');

	if (arglong.active) std::cout << "Argument Long arready active.\n";

	auto res = PxArg::parseArgs(v, {&arg1, &arg2, &arg3, &arglong, &argval});

	for (auto &i : res.assert()) {
		std::cout << i << " ";
	}
	std::cout << "\n";
	if (arg1.active) std::cout << "Argument 1 specified.\n";
	if (arg2.active) std::cout << "Argument 2 specified.\n";
	if (arg3.active) std::cout << "Argument 3 specified.\n";
	if (arglong.active) std::cout << "Argument Long specified.\n";
	if (argval.active) std::cout << "Argument Value specified: "<< argval.value << "\n";
	
}
