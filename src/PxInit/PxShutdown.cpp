
#include <csignal>
#include <cstring>
#include <filesystem>
#include <linux/reboot.h>
#include <sys/signal.h>
#include <PxLog.hpp>
#include <PxMount.hpp>
#include <errno.h>
#include <sys/syscall.h>
#include <PxShutdown.hpp>
#include <PxState.hpp>
#include <sys/wait.h>
#include <config.hpp>
#include <vector>

extern pid_t daemon_pid;
bool shuttingDown = false;
bool errorsEncountered = false;

#define ALLOWED_FS {"proc", "sysfs", "devtmpfs", "tmpfs", "devpts"}

static inline void remount_as_readonly(std::vector<PxMount::FsEntry> &filesystems) {
    for (auto& i : PxFunction::Reverse(filesystems)) {
        auto splitted = PxFunction::split(i.options, ",");

        if (PxFunction::contains(splitted, "ro"))     continue;
        if (PxFunction::startsWith(i.target, "/sys"))       continue;
        if (PxFunction::startsWith(i.target, "/proc"))      continue;
        if (PxFunction::startsWith(i.target, "/dev"))       continue;
        if (PxFunction::contains(ALLOWED_FS, i.type)) continue;

        auto status = PxMount::Mount(i.source, i.target, i.type, "remount,ro");
        if (status.eno) {
            PxLog::log.warn("warn: cannot remount "+i.target+": "+status.funcName+": "+strerror(status.eno));
            errorsEncountered = true;
        }
    }
}

static inline void unmount_all(std::vector<PxMount::FsEntry> &filesystems) {
    for (auto& i : PxFunction::Reverse(filesystems)) {
        if (i.target == "/")                                       continue;
        if (PxFunction::startsWith(i.target, "/sys"))       continue;
        if (PxFunction::startsWith(i.target, "/proc"))      continue;
        if (PxFunction::startsWith(i.target, "/dev"))       continue;
        if (PxFunction::contains(ALLOWED_FS, i.type)) continue;

        auto status = PxMount::Mount("", i.target, "", "remount,ro");
        if (status.eno) {
            PxLog::log.warn("warn: cannot unmount "+i.target+": "+status.funcName+": "+strerror(status.eno));
            errorsEncountered = true;
        }
    }
}

static inline void signal_all_processes(int signal) {
    std::vector<pid_t> pids;
    int iterations = 0;
    while (iterations++ < 10) {
        pids.clear();
        for (auto &i : std::filesystem::directory_iterator("/proc")) {
            pid_t pid = std::atoi(i.path().filename().c_str());

            // discard if invalid
            if (pid == 0) continue;
            if (pid == 1) continue;

            auto resstat = PxState::fget("/proc/"+std::to_string(pid)+"/stat");
            
            // stopped between listing and now
            if (resstat.eno != 0) continue;

            auto stat = PxFunction::split(resstat.assert());
            if (stat.size() < 3) continue;
            auto rstat = stat[2];

            if (rstat == "Z") continue;

            pids.push_back(pid);
        }
        if (pids.empty()) return;
        for (auto pid : pids) {
            // kill process
            kill(pid, signal);
        }
        if (signal != SIGKILL) return;
    }
}

void shutdown(int poweropt) {
	if (shuttingDown) return;
	PxLog::log.unsuppress();
	shuttingDown = true;

	signal(SIGINT, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
    
    // sync first so disk cache is already flushed by shutdown
	sync();

    // Kill all processes
    PxLog::log.info("Sending TERM signal to all processes");
    signal_all_processes(SIGTERM);

    // give processes a bit to clean up
    usleep(1000000);

    PxLog::log.info("Sending KILL signal to all processes");
    signal_all_processes(SIGKILL);

    // kill daemon
	kill(daemon_pid, SIGKILL);

    // load table
	PxMount::Table table;
	table.LoadFromFile("/etc/mtab");
	auto list_res = table.List();

	if (list_res.eno) {
        // warn user
		PxLog::log.warn("warn: cannot list filesystems: "+list_res.funcName+": "+strerror(list_res.eno));
		PxLog::log.warn("Data loss is possible!");
		usleep(6000000);

        // sync disks
		PxLog::log.info("Syncing disks...");
		sync();

        // wait a little bit
		PxLog::log.info("waiting a little bit...");
		usleep(1000000);
	} else {
		auto list = list_res.assert();

        // remount filesystems as readonly
		PxLog::log.info("Remounting all filesystems as read-only...");
        remount_as_readonly(list);

        // sync
		PxLog::log.info("Syncing disks...");
		sync();

        // wait a little bit
		PxLog::log.info("Waiting a little bit...");
		usleep(1000000);

        // unmount all filesystems
		PxLog::log.info("Unmounting all filesystems except root...");
        unmount_all(list);
	}

    if (errorsEncountered) {
        #ifdef PXFLAGDEBUG
        PxLog::log.warn("Errors were encountered. Waiting 15s for debug purposes...");
        usleep(15000000);
        #else
        PxLog::log.warn("Errors were encountered, waiting for a bit longer...");
        usleep(2000000);
        #endif
    }

	shutdown_call(poweropt);
}

PxResult::Result<void> sys_reboot(int command) {
	if (syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, command, NULL) != 0) {
		return PxResult::Result<void>("sys_reboot / syscall(SYS_reboot, ...)", errno);
	}
	return PxResult::Null;
}

void shutdown_call(int poweropt) {
	PxResult::Result<void> res;
	switch (poweropt) {
		case 0:
			res = sys_reboot(LINUX_REBOOT_CMD_POWER_OFF);
			if (res.eno) { errno = res.eno; perror(res.funcName.c_str()); }
			break;
		case 1:
			res = sys_reboot(LINUX_REBOOT_CMD_RESTART);
			if (res.eno) { errno = res.eno; perror(res.funcName.c_str()); }
			break;
	}
	res = sys_reboot(LINUX_REBOOT_CMD_HALT);
	res.assert("shutdown");
	PxLog::log.error("An error occurred trying to power the system off.");
	PxLog::log.info("It is now safe to turn off your computer.");
	while(1) usleep(1000000);
}