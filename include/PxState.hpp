
#include <string>
#include "PxResult.hpp"

#ifndef PXSTATE_HPP
#define PXSTATE_HPP

namespace PxState {
	PxResult::Result<void> fput(std::string filename, std::string content);
	PxResult::Result<std::string> fget(std::string filename);
	PxResult::Result<std::string> fgetp(std::string filename);
};
#endif
