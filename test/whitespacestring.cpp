#include <PxWritableConfig.hpp>

int main() {
	std::string value = "   This string will be trimmed\n";
	auto trimmed = PxConfig::xtrim(value);
	std::cout << "\"" << trimmed.ltrim << "\"\n";
	std::cout << "\"" << trimmed.rtrim << "\"\n";
	std::cout << "\"" << trimmed.content->get() << "\"\n";
}
