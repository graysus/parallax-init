
#ifndef PXCONTROL
#define PXCONTROL

#include <string>
struct PxFlags {
	bool now = false;
};

std::string getsvman();
extern PxFlags fl;

#endif