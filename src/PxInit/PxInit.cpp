
#include <PxState.hpp>
#include <PxMount.hpp>
#include <PxIPCServ.hpp>
#include <PxIPC.hpp>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <PxProcess.hpp>
#include <config.hpp>

pid_t daemon_pid;
PxIPC::Server<char> serv;
PxJob::JobServer jobs;

PxResult::Result<void> sys_reboot(int command) {
	if (syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, command, NULL) != 0) {
		return PxResult::Result<void>("sys_reboot / syscall(SYS_reboot, ...)", errno);
	}
	return PxResult::Null;
}

bool shuttingDown = false;

void shutdown(int poweropt) {
	if (shuttingDown) return;
	shuttingDown = true;
	kill(daemon_pid, SIGKILL);
	PxMount::Table table;
	table.LoadFromFile("/etc/mtab");
	auto list_res = table.List();
	sync();
	if (list_res.eno) {
		errno = list_res.eno;
		perror(("warn: cannot list filesystems: "+list_res.funcName).c_str());
		perror("Data loss is possible!");
		usleep(6000000);
		std::cout << "Syncing disks...\n";
		sync();
		usleep(1000000);
	} else {
		std::cout << "Remounting all filesystems as read-only...\n";
		auto list = list_res.assert();
		for (auto& i : list) {
			if (i.target == "/proc" && i.type == "proc") continue;
			auto status = PxMount::Mount("", i.target, "", "remount,ro");
			if (status.eno) {
				errno = status.eno;
				perror(("warn: cannot remount "+i.target+": "+status.funcName).c_str());
			}
		}
		std::cout << "Syncing disks...\n";
		sync();
		std::cout << "waiting a little bit...\n";
		usleep(1000000);
		std::cout << "Unmounting all filesystems except root...\n";
		for (auto& i : list) {
			if (i.target == "/") continue;
			if (i.target == "/proc" && i.type == "proc") continue; // LIAR!
			auto status = PxMount::Mount("", i.target, "", "remount,ro");
			if (status.eno) {
				errno = status.eno;
				perror(("warn: cannot unmount "+i.target+": "+status.funcName).c_str());
			}
		}
	}
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
	std::cout << "An error occurred trying to power the system off.\nIt is now safe to turn off your computer.";
	while(1) usleep(1000000);
}

void CheckCommand(int argc, const char **argv, std::string target, std::string command) {
	std::string path = argc > 1 ? argv[1] : "";
//	PxLog::log.info((std::string)argv[0]+", "+path);
	if ((std::string)argv[0] == target || (std::string)path == target || PxFunction::endsWith(argv[0], "/"+target)) {
		{
			PxIPC::Client cli;
			cli.Connect("/run/parallax-svman.sock").assert("PxInit (as shutdown)");
			cli.Write(command+'\0').assert("PxInit (as shutdown)");
		} // Closes client
		exit(0);
	}
	
}

PxResult::Result<void> OnCommand(PxIPC::EventContext<char> *ctx) {
	//std::cout << "Got command: " << ctx->command << "\n";
	if (ctx->command == "poweroff") shutdown(0);
	if (ctx->command == "reboot") shutdown(1);
	if (ctx->command == "halt") shutdown(2);
	return PxResult::Null;
}

void cad(int sig) {
	shutdown(1);
}

int main(int argc, const char *argv[]) {
	if (getpid() != 1) {
		CheckCommand(argc, argv, "poweroff", "target poweroff");
		CheckCommand(argc, argv, "reboot", "target reboot");
		CheckCommand(argc, argv, "halt", "target halt");
		PxLog::log.error("Must be run as PID1.");
		if (getppid() == 1) {
			PxLog::log.info("To start init as PID1, run \"exec init\" instead.");
		}
		return 1;
	}

	PxMount::Mount("proc", "/proc", "proc").assert("main / mount proc");
	PxMount::Mount("tmpfs", "/run", "tmpfs").assert("main / mount run");
	PxMount::Mount("sysfs", "/sys", "sysfs").assert("main / mount sysfs");
	PxMount::Mount("tmpfs", "/tmp", "tmpfs").assert("main / mount tmpfs");
	PxMount::Mount("cgroup2", "/sys/fs/cgroup", "cgroup2").assert("main / mount cgroup");

	mkdir("/dev/pts", 0600);
	mkdir("/dev/shm", 0600);

	PxMount::Mount("devpts", "/dev/pts", "devpts").assert("main / mount devpts");
	PxMount::Mount("tmpfs", "/dev/shm", "tmpfs").assert("main / mount devshm");

#ifdef PXFLAGDEBUG
	// Enable core dumps
	system("ulimit -S -c unlimited");
#endif

	serv.Init("/run/parallax-pid1.sock").assert("main");
	serv.on_command = OnCommand;

	jobs.AddJob(std::make_shared<PxIPC::ServerJob<char>>(&serv));

	if (chown("/run/parallax-pid1.sock", 0, 0) < 0) {
		perror("parallax-pid1 / chown");
		return 1;
	}
	if (chmod("/run/parallax-pid1.sock", 0755) < 0) {
		perror("parallax-pid1 / chmod");
		return 1;
	}

	sys_reboot(LINUX_REBOOT_CMD_CAD_OFF);

	// Start the IPC server

	pid_t f = fork();
	if (f < 0) PxResult::Result<void>("main / fork", errno).assert();
	if (f) {
		signal(SIGINT, cad);
		while (true) {
			usleep(1000);
			jobs.tick();
		}
	} else {
		PxProcess::CloseFD();
		pid_t f = fork();
		if (!f) {
			if (execl("/usr/lib/parallax/PxLogger", "PxLogger", NULL) < 0) {
				perror("PxInit / execl");
			}
			while (true) usleep(10000);
		} else {
			usleep(10000);
			execl("/usr/lib/parallax/PxDaemon", "PxDaemon", NULL);
			while (true) usleep(10000);
		}
	}
}
