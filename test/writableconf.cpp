#include <PxWritableConfig.hpp>

int main() {
	auto conf = PxConfig::parseWritableLine("abc = def;ghi  # This is a comment!\n# Comment only.\nbcd=mno\nabc=jkl");
	std::cout << "conf->get()\n";
	std::cout << conf->get() << "\n";

	std::cout << "conf->find()\n";
	std::cout << conf->find("abc", 0)->get() << "\n";
	std::cout << conf->find("abc", 1)->get() << "\n";
	std::cout << conf->find("abc", 2)->get() << "\n";

	conf->insto("abc", 2)->me = "Mod_2";
	std::cout << "conf->get() (modified)\n";
	std::cout << conf->get() << "\n";
	
	std::cout << "conf->keys(), conf->countkey()\n";
	for (auto i : conf->keys()) {
		std::cout << i << " (" << conf->countkey(i) << ")\n";
	}
	return 0;
}
