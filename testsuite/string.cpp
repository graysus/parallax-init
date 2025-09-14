#include <PxFunction.hpp>
#include <PxTest.cpp>

std::string PxTestMain(int argc, const char **argv) {
	std::string report = "";
	std::string testType = argv[2];

	if (testType == "split-join") {
		std::string cool = "First,Second,Third,Fourth";
		auto v = PxFunction::split(cool, ",");
		for (auto &i : v) {
			report += "Value "+i+";";
		}
	} else if (testType == "trim") {
		std::string cool2 = "    much whitespace   \n\n\n";
		std::string cool3 = "    left whitespace";
		std::string cool4 = "no whitespace";
		std::string cool5 = "";
		report += PxFunction::trim(cool2) + "\n";
		report += PxFunction::trim(cool3) + "\n";
		report += PxFunction::trim(cool4) + "\n";
		report += PxFunction::trim(cool5) + "end";
	}
	return report;
}
