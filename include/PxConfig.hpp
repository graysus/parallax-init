#include <PxFunction.hpp>
#include <PxResult.hpp>
#include <PxLog.hpp>
#include <PxState.hpp>
#include <cerrno>
#include <cstdlib>
#include <map>

#ifndef PXCONF
#define PXCONF
namespace PxConfig {
	struct conf {
		std::map<std::string, std::string> properties;
		std::map<std::string, std::vector<std::string>> vec_properties;
		int maxargs = 9;
	};
	std::vector<std::string> Escape(std::string baseString, std::string argument = "", int msplit = 9);
	PxResult::Result<void> ConfSetValue(conf *c, std::string key, std::string value);
	PxResult::Result<conf> ReadConfig(std::string filename, std::string argument = "");
};
#endif
