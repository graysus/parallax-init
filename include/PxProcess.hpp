
#include <PxLog.hpp>
#include <PxResult.hpp>
#include <PxFunction.hpp>
#include <unistd.h>

#ifndef PXPROC
#define PXPROC

namespace PxProcess {
	void CloseFD(bool include_io = false);
}

#endif
