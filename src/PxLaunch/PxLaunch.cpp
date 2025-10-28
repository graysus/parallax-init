#include <PxFunction.hpp>
#include <PxIPC.hpp>
#include <PxLog.hpp>
#include <PxJob.hpp>
#include <PxResult.hpp>
#include <csignal>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <PxProcess.hpp>
#include <unistd.h>
#include <PxService.hpp>
#include <vector>

enum Stages {
	Starting, Running, Ending, Erroring
};

// TODO: add fail commands, send fail signal, etc.
// TODO: better error handling

PxIPC::Client cli;
std::string ShellBegin, ShellRun, ShellEnd, ShellErr, ServName;
bool Killable, IsUser, IntrFlag;
uid_t UserUID;
int curStage = Starting;
int curpid;

signed int wp(int pid) {
	int wstatus;
	curpid = pid;
	while (true) {
		if (waitpid(pid, &wstatus, WNOHANG) != 0) {
			return WEXITSTATUS(wstatus);
		}
		if (IntrFlag) {
			IntrFlag = false;
			return -1;
		}
		usleep(10000);
	}
}

signed int runShell(std::string shell, int *pid) {
	int f = fork();
	if (f < 0) return -1;
	if (!f) {
		PxProcess::CloseFD();
		// safety first
		unsetenv("PATH");
		unsetenv("LD_LIBRARY_PATH");
		if (shell.find("\n") != std::string::npos) {
			std::vector<std::string> execV = {};
			if (PxFunction::startsWith(shell, "#!")) {
				auto shebangLine = shell.substr(2, shell.find("\n")-2);
				auto splitted = PxFunction::split(shebangLine, " ", 1);

				execV.push_back(splitted[0]);

				for (auto i : splitted) {
					execV.push_back(i);
				}

				// TODO: better
				execV.push_back("-lc");
				execV.push_back(shell);
			} else {
				// TODO: maybe better?
				execV.push_back("/bin/sh");
				execV.push_back("/bin/sh");
				execV.push_back("-lc");
				execV.push_back(shell);
			}

			PxProcess::Exec(execV);
		} else {
			PxProcess::Exec("/bin/sh", "sh", "-lc", ("exec "+shell).c_str());
		}
		exit(1);
	} else {
		if (pid != NULL) {
			*pid = f;
		}
		return wp(f);
	}
}
unsigned int runShellNoSig(std::string shell, int *pid) {
	pid_t lpid;
	if (pid == NULL) {
		pid = &lpid;
	}
	int res = runShell(shell, pid);
	if (res > -1) return res;

	while (-1 == (res = wp(*pid)))
		;

	return res;
}

void IntHandle(int sig) {
	IntrFlag = true;
	return;
}
void PipeHandle(int sig) {
	// Do some really important work
}

int main(int argc, const char *argv[]) {
	auto args = PxFunction::vectorize(argc, argv);
	
	if (args.size() < 9) {
		PxLog::log.error("8 arguments required.");
		return 1;
	}
	ServName = args[6];
	IsUser = args[7] == "true";
	UserUID = std::atoi(args[8].c_str());

	PxService::Service svc(ServName, NULL, IsUser, UserUID);
	svc.reload();
	
	// If set to true: Child program will be SIGKILL-ed
	// If set to false: Child program will be SIGTERM-ed
	Killable = svc.getProperty("KillOnStop", "false") == "true";

	ShellBegin = svc.getProperty("StartCommand", "true");
	ShellRun = svc.getProperty("Command", "hang");
	ShellEnd = svc.getProperty("StopCommand", "true");
	ShellErr = svc.getProperty("FailCommand", "true");

	signal(SIGINT, IntHandle);
	signal(SIGPIPE, PipeHandle);


	if (IsUser) {
		cli.Connect("/run/parallax-svman-u" + std::to_string(UserUID) + ".sock").assert("PxLaunch");
	} else {
		cli.Connect("/run/parallax-svman.sock").assert("PxLaunch");
	}

	pid_t curpid;

	curStage = Starting;

	{
		pid_t pid;
		bool stop = false;
		int res = runShell(ShellBegin, &pid);

		// if interrupted, wait for the start command to run, then run the stop command
		if (res < 0) stop = true;
		while (res < 0) res = wp(pid);
		if (res != 0) goto fail;
		if (stop) goto stop;
	}


	curStage = Running;
	cli.Write("finstart "+ServName+'\0').assert();
	if (ShellRun == "hang") {
		curpid = 0;
		while (!IntrFlag) {
			usleep(50000);
		}
		IntrFlag = false;
	} else {
		bool killed = false;
		int res = runShell(ShellRun, &curpid);
		while (res < 0) {
			kill(curpid, Killable ? SIGKILL : SIGTERM);
			killed = true;
			res = wp(curpid);
		}
		if (res > 0 && !killed) {
			goto fail;
		}
	}

stop:
	curStage = Ending;
	cli.Write("servfin "+ServName+'\0');
	if (runShellNoSig(ShellEnd, NULL) != 0) {
		goto fail;
	}
	cli.Write("finstop "+ServName+'\0');
	return 0;

fail:
	curStage = Erroring;
	cli.Write("servfail "+ServName+'\0');
	runShellNoSig(ShellErr, NULL);
	cli.Write("finfail "+ServName+'\0');

	return 0;
}
