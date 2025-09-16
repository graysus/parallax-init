
#include <PxResult.hpp>

#ifndef PXSHUTDOWN
#define PXSHUTDOWN

extern bool shuttingDown;

void shutdown(int poweropt);
void shutdown_call(int poweropt);
PxResult::Result<void> sys_reboot(int command);

#endif