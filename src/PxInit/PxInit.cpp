
#include <PxState.hpp>
#include <PxMount.hpp>
#include <PxIPCServ.hpp>
#include <PxIPC.hpp>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <PxProcess.hpp>
#include <config.hpp>
#include <sys/wait.h>
#include <PxShutdown.hpp>

pid_t daemon_pid;
PxIPC::Server<char> serv;
PxJob::JobServer jobs;

class CollectorJob : public PxJob::Job {
public:
	CollectorJob() {}
	void onerror(PxResult::Result<void> err) override {
		PxJob::Job::onerror(err);
		failed = false;
		finished = false;
		finishing = false;
	}
	PxResult::Result<void> tick() override {
		for (auto &i : std::filesystem::directory_iterator("/proc")) {
			pid_t pid = std::atoi(i.path().filename().c_str());

			// discard if invalid
			if (pid == 0) continue;

			auto resstat = PxState::fget("/proc/"+std::to_string(pid)+"/stat");
			
			// stopped between listing and now
			if (resstat.eno == ENOENT) continue;
			PXASSERTM(resstat, "CollectorJob::tick");

			auto stat = PxFunction::split(resstat.assert());
			if (stat.size() < 4) return PxResult::FResult("CollectorJob::tick (invalid value of /proc/.../stat)", EINVAL);
			auto rstat = stat[2];

			if (stat[3] != "1") continue;
			if (rstat == "Z") {
				// collect zombie
				int wstatus;
				waitpid(pid, &wstatus, 0);
				PxLog::log.info("zombied process "+std::to_string(pid)+" exited with WSTATUS = "+std::to_string(wstatus));
			}
		}
		return PxResult::Null;
	}
};


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
CollectorJob cj;

static void onchild(int _) {
	cj.tick();
}

int main(int argc, const char *argv[]) {
#ifdef PXFLAGDEBUG
	signal(SIGINT, SIG_IGN);
	kill(getpid(), SIGINT);
#endif
	signal(SIGCHLD, onchild);
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
	
	PxLog::log.suppress(false);

	sys_reboot(LINUX_REBOOT_CMD_CAD_OFF);

	// Start the IPC server

	pid_t f = fork();
	if (f < 0) PxResult::FResult("main / fork", errno).assert();
	if (f) {
		signal(SIGINT, cad);
		while (true) {
			usleep(5000);
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
