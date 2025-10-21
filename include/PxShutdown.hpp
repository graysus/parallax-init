
#include <PxResult.hpp>
#include <set>

#ifndef PXSHUTDOWN
#define PXSHUTDOWN

extern bool shuttingDown;
extern std::set<pid_t> ignorePID;

void shutdown(int poweropt);
void shutdown_call(int poweropt);
PxResult::Result<void> sys_reboot(int command);

#endif