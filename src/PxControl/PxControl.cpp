
#include "PxControl.hpp"
#include <PxFunction.hpp>
#include <PxIPC.hpp>
#include <PxResult.hpp>
#include <PxLog.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>
#include <PxService.hpp>
#include <PxArg.hpp>
#include <config.hpp>

typedef PxResult::Result<void>(*CmdCallback)(std::vector<std::string> args);

bool isUser = false;
uid_t uid = 0;

struct PxCommand {
	int minArgs;
	int maxArgs;
	bool needsRoot;
	bool acceptsUser;
	CmdCallback callback;
	std::string description;
};

PxFlags fl = {};

std::string getsvman() {
	return isUser ? "/run/parallax-svman-u"+std::to_string(uid)+".sock" : "/run/parallax-svman.sock";
}
PxResult::Result<void> CmdStart(std::vector<std::string> args);
PxResult::Result<void> CmdStop(std::vector<std::string> args);
PxResult::Result<void> CmdRestart(std::vector<std::string> args);

PxResult::Result<void> ensrv(std::string svname, bool enabled) {
	PxService::Service sv(svname, NULL, isUser, uid);
	auto res = sv.reload();
	if (res.eno == ENOENT) {
		PxLog::log.error("Service "+svname+" does not exist!");
		exit(1);
	} else if (res.eno) return res.merge("CmdEnable");
	bool done = false;
	std::vector<std::string> targets;
	sv.conf.ReadValue("Target", targets);
	for (auto &i : targets) {
		done = true;
		PxService::Target tgt(i, NULL, isUser, uid, false);
		int err;

		if (enabled) {
			for (auto &candidate : tgt.getSearchLocs(false)) {
				auto res = PxFunction::mkdirs(candidate+"/service-enable");
				if (res.eno != 0) {
					errno = res.eno;
					continue;
				}
				err = creat((candidate+"/service-enable/"+svname).c_str(), 0644);
				if (err >= 0) break;
			}
			if (err < 0) return PxResult::Result<void>("ensrv / creat", errno);
		} else {
			for (auto &candidate : tgt.getSearchLocs(false)) {
				err = remove((candidate+"/service-enable/"+svname).c_str());
				if (err >= 0) break;
			}
			if (err < 0 && errno != ENOENT) return PxResult::Result<void>("ensrv / remove", errno);
		}

		if (!enabled) {
			// err = creat(pathd1.c_str(), 0644) < 0;
			// if (err < 0) err = creat(pathd2.c_str(), 0644);
			// if (err < 0) return PxResult::Result<void>("ensrv / creat", errno);
		} else {
			for (auto &candidate : tgt.getSearchLocs(false)) {
				err = remove((candidate+"/service-disable/"+svname).c_str());
				if (err >= 0) break;
			}
			if (err < 0 && errno != ENOENT) return PxResult::Result<void>("ensrv / remove", errno);
		}
		
	}
	if (!done) {
		PxLog::log.error("Service has no 'Target' set. This likely means that the service shouldn't be enabled or disabled.");
	}
	return PxResult::Null;
}

PxResult::Result<void> CmdEnable(std::vector<std::string> args) {
	for (auto &i : args) {
		auto res = ensrv(i, true);
		if (res.eno) return res;
	}
	PxLog::log.info("Enabled service(s) successfully.");
	if (fl.now) {
		return CmdStart(args);
	}
	return PxResult::Null;
}

PxResult::Result<void> CmdDisable(std::vector<std::string> args) {
	for (auto &i : args) {
		auto res = ensrv(i, false);
		if (res.eno) return res;
	}
	PxLog::log.info("Disabled service(s) successfully.");
	if (fl.now) {
		return CmdStop(args);
	}
	return PxResult::Null;
}
PxResult::Result<void> CmdStatus(std::vector<std::string> args) {
	PxIPC::Client cli;
	auto res = cli.Connect(getsvman());
	if (res.eno != 0) return res;

	for (auto &i : args) {
		auto res1 = cli.Write("query "+i+" status");
		if (res1.eno != 0) return res1;
		auto res2 = cli.Read();
		if (res2.eno != 0) return PxResult::Clear(res2);

		auto query_status = res2.assert();

		if (PxFunction::startsWith(query_status, "error ")) {
			auto splitted = PxFunction::split(query_status, " ", 1);
			PxLog::log.error("Error getting status of "+i+": "+splitted[1]);
			continue;
		}
		
		std::string statusline;
		if (args.size() <= 1)
			statusline = "Status:";
		else
			statusline = "Status of "+i+':';
		PxLog::log.info(statusline+' '+query_status);
	}
	return PxResult::Null;
}

PxResult::Result<void> CmdList(std::vector<std::string> _) {
	PxIPC::Client cli;
	PXASSERTM(cli.Connect(getsvman()), "PxControl / CmdList");
	PXASSERTM(cli.Write("list"), "PxControl / CmdList");
	auto res = cli.Read();
	PXASSERTM(res, "PxControl / CmdList");
	auto splitted = PxFunction::split(res.assert(), "\n");

	for (auto i : splitted) {
		if (i.empty()) continue;
		auto splcmd = PxFunction::split(i);
		if (splcmd[0] == "service") {
			PXASSERTM(cli.Write("query "+splcmd[1]+" status"), "PxControl / CmdList");
			auto response = cli.Read();
			PXASSERTM(response, "PxControl / CmdList");
			auto stat = response.assert();

			auto basename = splcmd[1];
			basename.reserve(25);
			while (basename.length() < 25) basename.push_back(' ');

			PxLog::log.println(basename + stat);
		}
	}

	return PxResult::Null;
}

PxResult::Result<void> CmdVersion(std::vector<std::string> _) {
	PxLog::log.info("Parallax version " PXVER);
	PxLog::log.info("pxctl is a part of the parallax-init project.");
	PxLog::log.info("parallax-init is free software; you may redistribute it under the terms of the MIT License.");
	PxLog::log.info("Type \"pxctl license\" for more information.");
	return PxResult::Null;
}

PxResult::Result<void> CmdLicense(std::vector<std::string> _) {
	PxLog::log.println(PXLICENSE);
	return PxResult::Null;
}

std::map<std::string, PxCommand> commands = {
	{"start", {1, -1, false, true, CmdStart, "Start a service"}},
	{"stop", {1, -1, false, true, CmdStop, "Stop a service"}},
	{"restart", {1, -1, false, true, CmdRestart, "Restart a service"}},
	{"enable", {1, -1, false, true, CmdEnable, "Enable a service at boot time"}},
	{"disable", {1, -1, false, true, CmdDisable, "Disable a service at boot time"}},
	{"status", {1, -1, false, true, CmdStatus, "Get the status of a service"}},
	{"list-loaded", {0, 0, false, true, CmdList, "List all loaded services"}},
	{"version", {0, 0, false, false, CmdVersion, "Get the Parallax version"}},
	{"license", {0, 0, false, false, CmdLicense, "Prints the Parallax license"}}
};

std::vector<std::string> ordered_commands = {
	"start", "stop", "restart", "enable", "disable", "status", "list-loaded", "version", "license"
};

int main(int argc, const char *argv[]) {
	auto raw_args = PxFunction::vectorize(argc, argv);

	PxArg::Argument help("help", 'h', "Display this message");
	PxArg::Argument isuser("user", 'u', "Send commands to user daemon");
	PxArg::Argument now("now", 'n', "Start/stop a service in addition to enabling/disabling it.");
	PxArg::SelectArgument command("COMMAND", "The command to send to the Parallax daemon");
	PxArg::ArgParser parser({&command}, {&help, &isuser, &now});

	for (auto &i : ordered_commands) {
		command.addOption(i, commands[i].description);
	}
	for (auto &i : commands) {
		if (PxFunction::contains(ordered_commands, i.first)) continue;

		#ifdef PXFLAGDEBUG
			std::cout << "ordered_commands does not include " << i.first << "\n";
			exit(1);
		#else
			command.addOption(i.first, i.second.description);
		#endif
	}

	parser.cmdline = argv[0];
	parser.description = "Control the Parallax daemon.";

	auto res_args = parser.parseArgs(raw_args);
	if (help.active) {
		parser.printHelp();
		return 0;
	}
	if (res_args.eno != 0) {
		PxLog::log.error("Bad argument.");
		PxLog::log.info("Type pxctl --help for help.");
		return 1;
	}

	fl.now = now.active;

	if (isuser.active) {
		isUser = true;
		uid = getuid();
	} else {
		isUser = false;
		uid = 0;
	}
	
	auto args = res_args.assert();
	if (commands.count(command.value) == 0) {
		PxLog::log.error("Invalid command.");
		PxLog::log.info("Type pxctl --help for help.");
		return 1;
	}
	auto cmd = commands[command.value];
	if (cmd.needsRoot && getuid() != 0) {
		PxLog::log.error("Must be run as root: "+command.value);
		return 1;
	}
	if (cmd.acceptsUser && getuid() != uid) {
		PxLog::log.error("Must be run as relevant user: "+command.value);
		return 1;
	}
	if (args.size() < cmd.minArgs) {
		PxLog::log.error("Too few arguments to "+command.value);
		return 1;
	}
	if (cmd.maxArgs != -1 && args.size() > cmd.maxArgs+cmd.minArgs) {
		PxLog::log.error("Too many arguments to "+command.value);
		return 1;
	}
	auto res = cmd.callback(args);
	if (res.eno) {
		PxLog::log.error("Error: "+res.funcName+": "+strerror(res.eno));
		return 1;
	}
}
