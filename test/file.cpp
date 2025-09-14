#include <PxState.hpp>
#include <PxResult.hpp>
#include <PxMount.hpp>
#include <iostream>

int main() {
	PxMount::Unmount("/mnt");
	auto tab = PxMount::Table();
	tab.LoadFromFile("/etc/mtab").assert("test / main");
	auto filesystems = tab.List().assert("test / main");
	for (auto& i : filesystems) {
		std::cout << i.source << "\n";
	}
	return 0;
}
