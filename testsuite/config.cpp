#include <PxConfig.hpp>
#include <PxTest.cpp>
#include <PxFunction.hpp>
#include <string>
#include <vector>

std::string PxTestMain(int argc, const char *argv[]) {
	std::vector<std::string> v = PxFunction::vectorize(argc, argv);

	std::string params = v[3];
	auto config = PxConfig::ReadConfig(v[2], params).assert("config / PxTestMain");

	std::string report;
	
	for (auto &i : config.QuickReadVec("ReportVariable")) {
		report+="VAR_"+i+";";
		for (auto &m : config.QuickReadVec(i)) {
			report+=m+";";
		}
	}

	return report;
}
