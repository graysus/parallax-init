
// This code worked first try, somehow
#include <string>
#include <PxFunction.hpp>
#include <PxResult.hpp>

#ifndef PXENV
#define PXENV

namespace PxEnv {
	void dumpStrings(std::string filename, std::vector<std::string>& strings);
	void loadStrings(std::string filename, std::vector<std::string>& strings);
	void dump(std::string filename);
	void load(std::string filename);
}
#endif
