
#include <PxServiceManager.hpp>
#include <PxJob.hpp>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <PxIPCServ.hpp>
#include <PxFunction.hpp>
#include <PxTarget.hpp>
#include <map>
#include <sys/stat.h>
#include <PxConfig.hpp>
#include <config.hpp>
#include <PxArg.hpp>
#include <PxEnv.hpp>
#include <vector>

enum Cmd {
	Start, Stop, FinishStart, FinishStop, ServDepend, ServRemoveDepend, ServFinish, ServFail, ServFinishFail, CascadeStop, Target, SpawnSession, Reference, Export, Query, List
};

struct PxCommandRange {
	Cmd cmdid;
	int min;
	int max;
};

std::map<std::string, PxCommandRange> CommandSizes = {
	{"start", {Cmd::Start, 1, 0}},
	{"stop", {Cmd::Stop, 1, 0}},
	{"finstart", {Cmd::FinishStart, 1, 0}},
	{"finstop", {Cmd::FinishStop, 1, 0}},
	{"servfin", {Cmd::ServFinish, 1, 0}},
	{"servfail", {Cmd::ServFail, 1, 0}},
	{"finfail", {Cmd::ServFinishFail, 1, 0}},
	{"servdep", {Cmd::ServDepend, 2, 0}},
	{"servrmdep", {Cmd::ServRemoveDepend, 2, 0}},
	{"cascade-stop", {Cmd::CascadeStop, 1, 0}},
	{"target", {Cmd::Target, 1, 0}},
	{"spawn", {Cmd::SpawnSession, 4, 0}},
	{"reference", {Cmd::Reference, 1, 0}},
	{"export", {Cmd::Export, 2, -1}},
	{"query", {Cmd::Query, 2, 0}},
	{"list", {Cmd::List, 0, 0}}
};

std::map<PxService::ServiceState, std::string> stateNames = {
	{PxService::ServiceState::Failed, 		"failed"},
	{PxService::ServiceState::FailStopping, "stopping due to failure"},
	{PxService::ServiceState::Stopping,     "stopping"},
	{PxService::ServiceState::Inactive,     "inactive"},
	{PxService::ServiceState::Running,      "running"},
	{PxService::ServiceState::Starting,     "starting"},
	{PxService::ServiceState::Waiting,      "waiting for dependencies to start"},
	{PxService::ServiceState::StopWaiting,  "waiting for dependencies to stop"}
};

PxService::ServiceManager mgr;
PxJob::JobServer jobs;
PxIPC::Server<char> serv;

// the refcount for this session
int refs = 0;

PxResult::Result<void> PxDOnCommand(PxIPC::EventContext<char> *ctx) {
	auto splitCommand = PxFunction::split(ctx->command, " ");
	if (splitCommand.size() < 0) {
		return PxResult::Result<void>("PxDOnCommand (not enough arguments)", EINVAL);
	}
	if (CommandSizes.count(splitCommand[0]) == 0) {
		return PxResult::Result<void>("PxDOnCommand (invalid command: "+splitCommand[0]+")", EINVAL);
	}
	auto cmdrange = CommandSizes[splitCommand[0]];
	if (cmdrange.max != -1 && (cmdrange.min+1 > splitCommand.size() || cmdrange.min+cmdrange.max+1 < splitCommand.size())) {
		return PxResult::Result<void>("PxDOnCommand (too many arguments)", EINVAL);
	}
	switch (cmdrange.cmdid) {
		case Start: {
			auto res = mgr.StartService(splitCommand[1], ctx->conn->handle).merge("PxDOnCommand");
			if (res.eno == ENOENT) ctx->reply("error nonexist");
			if (res.eno) ctx->reply("error internal-error "+std::to_string(res.eno)+" "+res.funcName);
			return res;
		}
		case Stop: {
			auto res = mgr.StopService(splitCommand[1], ctx->conn->handle).merge("PxDOnCommand");
			if (res.eno == ENOENT) ctx->reply("error nonexist");
			if (res.eno) ctx->reply("error internal-error "+std::to_string(res.eno)+" "+res.funcName);
			return res;
		}
		case FinishStart:
			return mgr.FinishStartService(splitCommand[1]).merge("PxDOnCommand");
		case FinishStop:
		case ServFinishFail: // same cauyse whgy not 
			return mgr.FinishStopService(splitCommand[1]).merge("PxDOnCommand");
		case ServDepend:
		case ServRemoveDepend:
			// mgr.services[""]
			return PxResult::Result<void>("PxDOnCommand (not implemented)", ENOTSUP); // TODO
		case ServFail:
			return mgr.FailService(splitCommand[1]);
		case CascadeStop:
			return mgr.CascadeStopService(splitCommand[1]);
		case ServFinish:
			return mgr.StopService(splitCommand[1]); // TODO: use a dedicated function for service finishing
		case Target:
			return mgr.setTarget(splitCommand[1]);
		case SpawnSession:{
			if (mgr.isUser) {
				return PxResult::Null;
			}

			int f = fork();
			if (f < 0) {
				return PxResult::Result<void>("fork", errno);
			}
			if (f == 0) {
				// child
				PxProcess::CloseFD();

				PxFunction::wrap("setgid", setgid(std::atoi(splitCommand[2].c_str()))).assert();
				PxFunction::wrap("setuid", setuid(std::atoi(splitCommand[1].c_str()))).assert();

				PxEnv::load(splitCommand[4]);
				remove(splitCommand[4].c_str());
				putenv(strdup(("HOME="+splitCommand[3]).c_str()));

				execl("/proc/self/exe", "PxDaemon", "--user", NULL);
			}
			return PxResult::Null;
		}
		case Reference: {
			if (!mgr.isUser) {
				return PxResult::Null;
			}
			if (splitCommand[1] == "inc") {
				refs++;
			} else {
				refs--;
				if (refs <= 0) {
					return mgr.setTarget(splitCommand[1]);
				}
			}
			return PxResult::Null;
		}
		case Export: {
			std::string val = "";
			for (auto i = splitCommand.begin() + 2; i != splitCommand.end(); i++) {
				val += *i + " ";
			}
			val = val.substr(0, val.length()-1);
			mgr.exports[splitCommand[1]] = splitCommand[1]+"="+val;
			return PxResult::Null;
		}
		case Query: {
			std::string svcname = splitCommand[1];
			std::string mode = splitCommand[2];

			if (mode == "status") {
				auto svcres = mgr.ReloadService(svcname);
				if (svcres.eno == ENOENT) {
					ctx->reply("error no-such-service").assert();
					return PxResult::Null;
				} else if (svcres.eno != 0) {
					ctx->reply("error internal error\0").assert();
					return PxResult::Clear(svcres);
				}
				auto svc = svcres.assert();
				ctx->reply(stateNames[svc->status]+'\0').assert();
			} else {
				ctx->reply("error invalid command\0").assert();
			}
			return PxResult::Null;
		}
		case List: {
			std::string listing = "";
			for (auto i : mgr.services) {
				listing += "service "+i.first + "\n";
			}
			ctx->reply(listing);
			return PxResult::Null;
		}
	}
	return PxResult::Result<void>("PxDOnCommand (internal error / control reached end of function)", EFAULT);
}

std::string GetOSName(std::string &regName) {
	// This includes the color, by the way.
	
	auto os_res = PxConfig::ReadConfig("/etc/os-release");
	if (os_res.eno == ENOENT) return "Linux";
	else if (os_res.eno) {
		PxLog::log.error("Error while getting OS info: "+os_res.funcName+": "+strerror(os_res.eno));
		return "Linux";
	}
	auto os = os_res.assert();

	std::vector<std::string> ansicolors_read;

	os.ReadValue("ANSI_COLOR", ansicolors_read);

	auto ansicolors = PxFunction::trim(PxFunction::join(ansicolors_read, ";"), "\"'");
	regName = os.QuickRead("PRETTY_NAME");
	return "\x1b["+ansicolors+"m"+PxFunction::trim(regName, "\"'")+"\x1b[0m";
}
void InitLog() {
	PxFunction::wrap("seteuid", seteuid(0)).assert("InitLog");
	PxIPC::Client cli;
	PxFunction::waitExist("/run/parallax-logger.sock");

	std::string prefix, suffix;
	if (mgr.isUser) {
		prefix = "user:" + std::to_string(mgr.uid) + ":";
		suffix = "-uid" + std::to_string(mgr.uid);
	} else {
		prefix = "";
		suffix = "";
	}

	cli.Connect("/run/parallax-logger.sock");
	cli.Write((std::string)"mklog pxdaemon"+suffix+" "+prefix+"daemon"+'\0');
	bool failed = !PxFunction::waitExist("/run/logger/pxdaemon");
	if (failed) {
		PxLog::log.error("Failed to open log file! Starting anyway...");
		std::cout << "Failed to open log file! Starting anyway...\n";
	}
	int fd = open(("/run/logger/pxdaemon"+suffix).c_str(), O_WRONLY | O_TRUNC, 0644);
	if (fd < 0) {
		PxLog::log.error("Error: "+(std::string)strerror(errno));
		std::cout << ("Error: "+(std::string)strerror(errno)) << "\n";
		exit(1);
	}
	PxLog::log.linkfd = fd;
	PxFunction::wrap("seteuid", seteuid(mgr.uid)).assert("InitLog");
}

void onterm(int _) {
	auto res = mgr.setTarget("logout");
	if (res.eno) {
		PxLog::log.error("Error on logout handler: "+res.funcName+": "+strerror(res.eno));
	}
}

void onint(int _) {
	auto res = mgr.setTarget("reboot");
	if (res.eno) {
		PxLog::log.error("Error on ctrl+alt+del handler: "+res.funcName+": "+strerror(res.eno));
	}
}

std::string getmotd(std::string distro) {
	for (auto i : {
		"/etc/motd.conf",
		"/usr/lib/parallax/motd.conf"
	}) {
		auto res = PxConfig::ReadConfig(i, distro);
		if (res.eno == ENOENT) continue;
		if (res.eno) {
			PxLog::log.warn("Error when getting MOTD: "+res.funcName+": "+strerror(res.eno));
			continue;
		}

		auto motdconf = res.assert();
		auto motds = motdconf.QuickReadVec("motd");

		auto flrand = ((float)rand()) / (float)RAND_MAX;

		return motds[std::floor(flrand * motds.size())];
	}

	return "";
}

int main(int argc, const char* argv[]) {
	PxArg::Argument isuser("user", 0, "User manager", true);
	PxArg::ArgParser ps({}, {&isuser});

	ps.parseArgs(PxFunction::vectorize(argc, argv)).assert("PxDaemon");

	if (isuser.active) {
		if (geteuid() != 0) {
			PxLog::log.error("Parallax does not have the setuid bit set, please run: su -c \"chmod u+s /usr/lib/parallax/PxDaemon\"");
			return 1;
		}

		mgr.init(true, getuid());
	} else if (getuid() != 0) {
		PxLog::log.error("System managers must be run with root privileges.");
		return 1;
	} else {
		mgr.init(false, 0);
	}

	InitLog();
	if (mgr.isUser) {
		PxLog::log.suppress(false);
	}

	{
		std::string osname;
		PxLog::log.header("Welcome to "+GetOSName(osname)+"!");
		PxLog::log.header("Running Parallax version " PXVER);

		time_t curtime = time(NULL);
		srand(curtime);

		auto motd = getmotd(osname);
		if (!motd.empty()) {
			PxLog::log.subheader(motd);
		}
	}


	if (!mgr.isUser) {
		serv.Init("/run/parallax-svman.sock");

		if (chown("/run/parallax-svman.sock", 0, 0) < 0) {
			perror("parallax-svman / chown");
			return 1;
		}
		if (chmod("/run/parallax-svman.sock", 0755) < 0) {
			perror("parallax-svman / chmod");
			return 1;
		}
	} else {
		auto sockpath = "/run/parallax-svman-u" + std::to_string(getuid()) + ".sock";

		int oldid = mgr.uid;
		PxFunction::wrap("seteuid root", seteuid(0)).assert("main");

		serv.Init(sockpath);

		if (chown(sockpath.c_str(), oldid, oldid) < 0) {
			perror("parallax-svman / chown");
			return 1;
		}
		if (chmod(sockpath.c_str(), 0755) < 0) {
			perror("parallax-svman / chmod");
			return 1;
		}
		PxFunction::wrap("seteuid restore", seteuid(oldid)).assert("main");
	}

	if (mgr.isUser) {
		signal(SIGTERM, onterm);
	} else {
		signal(SIGINT, onint);
	}

	mgr.server = &serv;

	auto j = std::make_shared<PxIPC::ServerJob<char>>(&serv);
	jobs.AddJob(j);
	auto j2 = std::make_shared<PxJob::OscJob>(&PxLog::log);
	jobs.AddJob(j2);
	
	mgr.setTarget("boot");
	serv.on_command = PxDOnCommand;

	while (true) {
		usleep(5000);
		jobs.tick();
	}
}
