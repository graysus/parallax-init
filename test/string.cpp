#include <PxFunction.hpp>
#include <iostream>

int main() {
	std::cout << "Test 1: split\n";
	std::string cool = "First element,Second element,Third element,Fourth element";
	auto v = PxFunction::split(cool, ",");
	for (auto &i : v) {
		std::cout << i << " vec element\n";
	}

	std::cout << "Test 2: trim\n";
	std::string cool2 = "    This string had a lot of whitespace   \n\n\n";
	std::string cool3 = "    This string had some whitespace";
	std::string cool4 = "This string had no whitespace";
	std::cout << PxFunction::trim(cool2) << "\n";
	std::cout << PxFunction::trim(cool3) << "\n";
	std::cout << PxFunction::trim(cool4) << "\n";
}
