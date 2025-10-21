
#include <PxFunction.hpp>
#include <PxState.hpp>
#include <PxResult.hpp>
#include <PxJob.hpp>
#include <PxLog.hpp>
#include <PxIPC.hpp>
#include <PxConfig.hpp>
#include <PxProcess.hpp>
#include <PxServiceHelpers.hpp>
#include <PxTarget.hpp>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#ifndef PXSSVC
#define PXSSVC

namespace PxService {
	struct DependType {
		std::string name;
		bool optional;

		bool operator==(const DependType &other) {
			return name == other.name && optional == other.optional;
		}
	};

	class Service {
	public:
		std::string name;
		ServiceManager *mgr;
		// Deps refers to both dynamic and static dependencies.
		// For a list of static dependencies, use vec_properties
		std::vector<DependType> deps;
		std::vector<DependType> dependedBy;
		std::vector<size_t> subscribed_handles = {};
		ServiceOverrideState userOverride = ServiceOverrideState::Auto;
		ServiceState status = ServiceState::Inactive;
		bool following = false;
		bool failLocked = false;
		int logTask;
		int wk_pid;
		PxConfig::conf conf;

		bool isUser = false;
		uid_t uid = 0;
		bool privileged = false;

		Service(std::string name, ServiceManager *Mgr, bool isuser, uid_t uid) {
			this->name = name;
			mgr = Mgr;
			isUser = isuser;
			this->uid = uid;
		}
		std::string getProperty(std::string name, std::string def = "") {
			auto val = conf.QuickRead(name);
			return val.empty() ? def : val;
		}
		ServiceOverrideState isRunning() {
			switch (status) {
				case ServiceState::Inactive:
				case ServiceState::Failed:
					return Exclude;
				case ServiceState::Running:
					return Include;
				default:
					return Auto;
			}
		}
		bool shouldRun(bool def = false) {
			// Step 1: Check if system is shutting down.
			// System shutdown takes precedence over all other things.
			// Property "IgnoreShutdown" ignores this.
			bool isShutdown = GetTargetOf(mgr)->HasAttribute("system-shutdown") || GetTargetOf(mgr)->HasAttribute("daemon-shutdown");
			if (isShutdown && getProperty("IgnoreShutdown", "false") != "true") {
				return false;
			}
			if (failLocked) {
				return false;
			}
			// Step 2: Check if the service has been manually started or stopped.
			if (dependedBy.size() > 0) return true;
			switch (userOverride) {
				case ServiceOverrideState::Include:
					return true;
				case ServiceOverrideState::Exclude:
					return false;
				case ServiceOverrideState::Auto:
					break;
			}
			// Removed because it can cause conflicts with the target.
			/*if (!following) {
				if (targetIncludes(!isRunning()) == isRunning()) {
					following = true;
				} else {
					return isRunning();
				}
			}*/
			return targetIncludes(def);
		}

		bool targetIncludes(bool def = false) {
			// Step 3: Check for service file
			bool disable = GetTargetOf(mgr)->Disabled(name);
			bool enable = GetTargetOf(mgr)->Enabled(name);
			ServiceOverrideState targetOverride = disable ? ServiceOverrideState::Exclude :
							      enable ? ServiceOverrideState::Include :
							      ServiceOverrideState::Auto;
			switch (targetOverride) {
				case ServiceOverrideState::Include:
					return true;
				case ServiceOverrideState::Exclude:
					return false;
				case ServiceOverrideState::Auto:
					break;
			}
			return def;
		}
		PxResult::Result<void> dispatchState(std::string newstate) {
			for (auto hndl : subscribed_handles) {
				auto server = ServerOf(mgr);
				auto connh = server->getConnection(hndl);
				
				// client disconnected before dispatch
				if (connh.eno == EPIPE) continue;
				PXASSERTM(connh, "PxService::Service::dispatchState");
				PXASSERTM(server->Send(connh.assert(), "reached "+name+" "+newstate), "PxService::Service::dispatchState");
			}
			subscribed_handles.clear();
			return PxResult::Null;
		}
		PxResult::Result<void> start() {
			status = Waiting;
			// Add dependencies
			deps.erase(deps.begin(), deps.end());

			std::vector<std::string> depstr;
			conf.ReadValue("Depend", depstr);


			// Start dependencies
			for (auto &i : depstr) {
				bool optional = false;
				if (PxFunction::endsWith(i, "?")) {
					optional = true;
					i = i.substr(0, i.length()-1);
				}

				auto res = LoadServiceOf(mgr, i);
				if (res.eno && optional) continue;
				PXASSERTM(res, "PxService::Service::start");
				auto serv = res.assert();

				DependType depval = {
					.name = name,
					.optional = optional
				};
				DependType depval2 = {
					.name = i,
					.optional = optional
				};

				serv->dependedBy.push_back(depval);
				deps.push_back(depval2);

				auto res2 = serv->update();
				if (res2.eno && optional) {
					PxFunction::vecRemoveItem(&serv->dependedBy, depval);
					PxFunction::vecRemoveItem(&deps, depval2);
					continue;
				}
				PXASSERTM(res2, "PxService::Service::start");
				// TODO: add cleanup / fail status on fail
			}
			return update().replace().merge("PxService::Service::start");
		}
		PxResult::Result<void> raw_start() {
			status = ServiceState::Starting;
			if (logTask) PxLog::log.completeTask(logTask, PxLog::Partial);
			logTask = PxLog::log.newTask(new LogServiceStartTask(name));
			bool usingConsole = getProperty("UseConsole", "false") == "true";

			std::string logname, svname;
			if (isUser) {
				logname = "service-u" + std::to_string(uid) + "-"+ name;
				svname = "user:" + std::to_string(uid) + ":" + name;
			} else {
				logname = "service-" + name;
				svname = name;
			}
			auto logpath = "/run/logger/"+logname;

			// Create a log
			if (!usingConsole) {
				PxIPC::Client cli;

				PXASSERTM(PxFunction::wrap("seteuid", seteuid(0)), "PxService::Service::raw_start");

				PXASSERTM(cli.Connect("/run/parallax-logger.sock"), "PxService::Service::raw_start");
				PXASSERTM(cli.Write("mklog "+logname+" "+svname+'\0'), "PxService::Service::raw_start");

				PXASSERTM(PxFunction::wrap("seteuid", seteuid(getuid())), "PxService::Service::raw_start");
			}

			// Fork a PxLaunch
			int f = fork();
			if (f < 0) return PxResult::Result<void>("PxService::Service::raw_start / fork", errno);
			else if (!f) {
				auto resuid = PxFunction::wrap("seteuid", seteuid(0));
				if (resuid.eno != 0) exit(93);

				if (usingConsole) {
					PxProcess::CloseFD(false);
				} else {
					PxProcess::CloseFD(true);
					PxFunction::waitExist(logpath);
					open("/dev/null", O_TRUNC | O_RDONLY); // fd 0 (stdin)
					// /run/logger/service-svc1
					int out = openat(AT_FDCWD, logpath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600); // fd 1 (stdout)
					if (out < 0) {
						// that's just unfortunate, isn't it?
						exit(94);
					}
					if (dup(1) != 2) {
						std::cout << "Duplicating gives wrong output! Exiting...\n"; // fd 2 (stderr)
					}
					std::cout << "Service started!\n";
					std::cout.flush();
				}

				uid_t newuid = std::atoi(getProperty("UserID", std::to_string(uid)).c_str());
				if (!privileged) {
					newuid = uid;
				}

				// safety first
				PXASSERTM(PxFunction::wrap("setuid", setuid(newuid)), "raw_start / setuid");
				unsetenv("PATH");
				unsetenv("LD_LIBRARY_PATH");

				std::vector<std::string> environment;
				ReadExportsOf(mgr, environment);
				for (auto &i : environment) {
					putenv((char*)i.c_str());
				}

				execl("/usr/lib/parallax/PxLaunch", "PxLaunch",
					getProperty("StartCommand", "true").c_str(),
					getProperty("Command", "true").c_str(),
					getProperty("StopCommand", "true").c_str(),
					getProperty("FailCommand", "true").c_str(),
					getProperty("KillOnStop", "false").c_str(),
					name.c_str(),
					(isUser ? "true" : "false"),
					std::to_string(uid).c_str(), NULL
				);
				exit(1);
			}

			wk_pid = f;

			return PxResult::Null;
		}
		PxResult::Result<void> finishStart() {
			PxLog::log.completeTask(logTask, PxLog::Success);
			logTask = 0;
			status = PxService::Running;

			for (auto &i : dependedBy) {
				auto res = LoadServiceOf(mgr, i.name);
				PXASSERTM(res, "PxService::Service::finishStart");
				auto serv = res.assert();
				PXASSERTM(serv->update(), "PxService::Service::finishStart");
//				// TODO: cleanup, error handling (like before)
			}
			bool usingConsole = getProperty("UseConsole", "false") == "true";
			if (usingConsole) PxLog::log.suppress();
			CheckReach(mgr);

			// TODO: print error
			dispatchState("started");
			return PxResult::Null;
		}
		PxResult::Result<void> stop() {
			status = ServiceState::StopWaiting;
			if (failLocked)
				PxLog::log.error("Error status detected on "+name+", stopping...", "error-detected service "+name);

			// dispatch stop / finstop
			for (auto &i : dependedBy) {
				auto res = LoadServiceOf(mgr, i.name);
				if (res.eno) {
					PxLog::log.error("Error in stop: " + res.funcName + ": " + strerror(res.eno));
					continue;
				}
				auto serv = res.assert();
				auto res2 = serv->update();
				if (res2.eno) {
					PxLog::log.error("Error in stop: " + res2.funcName + ": " + strerror(res2.eno));
					continue;
				}
			}
			return PxResult::LvClear(update());
		}
		PxResult::Result<void> cascadeStop() {
			userOverride = ServiceOverrideState::Exclude;
			if (isRunning() != Include) return PxResult::Null;
			for (auto &i : dependedBy) {
				// don't stop an optional dependency
				if (i.optional) continue;

				auto res = LoadServiceOf(mgr, i.name);
				if (res.eno) {
					PxLog::log.error("Error in cascadeStop: " + res.funcName + ": " + strerror(res.eno));
					continue;
				}
				auto serv = res.assert();
				auto res2 = serv->cascadeStop();
				if (res2.eno) {
					PxLog::log.error("Error in cascadeStop: " + res2.funcName + ": " + strerror(res2.eno));
					continue;
				}
				// TODO: recursion protection
			}
			return PxResult::LvClear(update()).merge("PxService::Service::cascadeStop");
		}
		PxResult::Result<void> raw_stop() {
			bool usingConsole = getProperty("UseConsole", "false") == "true";
			if (usingConsole) PxLog::log.unsuppress();
			if (failLocked)
				status = ServiceState::FailStopping;
			else
				status = ServiceState::Stopping;

			if (logTask) PxLog::log.completeTask(logTask, failLocked ? PxLog::Fail : PxLog::Partial);
			logTask = PxLog::log.newTask(new LogServiceStopTask(name));

			kill(wk_pid, SIGINT);

			return PxResult::Null;
		}
		PxResult::Result<void> finishStop() {
			PxLog::log.completeTask(logTask, PxLog::Success);
			logTask = 0;
			if (failLocked) 
				status = PxService::Failed;
			else
				status = PxService::Inactive;

			// TODO: print error
			if (failLocked)
				dispatchState("failed");
			else
				dispatchState("stopped");


			for (auto &i : deps) {
				auto res = LoadServiceOf(mgr, i.name);
				PXASSERTM(res, "PxService::Service::finishStop");
				auto serv = res.assert();
				
				size_t idx = 0;
				for (auto d : serv->dependedBy) {
					if (d.name == name) break;
					idx++;
				}
				if (idx == serv->dependedBy.size()) {
					PxLog::log.error("failed to remove dependency "+name+" from "+i.name);
				} else {
					PxFunction::vecRemoveIndeces(&serv->dependedBy, idx);
				}

				// In case the service is only up as a dependency for this service
				serv->update();

				// TODO: add cleanup on fail (this should not fail, anyway)
				// TODO: error handling somehow
			}
			deps.erase(deps.begin(), deps.end());
			CheckReach(mgr);
			return PxResult::Null;
		}
		PxResult::Result<bool> update() {
			if (status == Waiting) {
				// Check if dependencies have started.
				for (auto &i : deps) {
					auto res = LoadServiceOf(mgr, i.name);
					if (res.eno && i.optional) continue;
					PXASSERTM(res, "PxService::Service::update");
					if (res.assert()->isRunning() == Exclude && i.optional) continue;
					if (res.assert()->isRunning() != Include) return PxResult::Result(false);
				}
				return PxResult::LvAttach(raw_start(), true);
			}
			if (status == StopWaiting) {
				// Check if dependents have stopped.
				for (auto &i : dependedBy) {
					auto res = LoadServiceOf(mgr, i.name);
					PXASSERTM(res, "PxService::Service::update");
					if (res.assert()->isRunning() == Include && i.optional) continue;
					if (res.assert()->isRunning() != Exclude) return PxResult::Result(false);
				}
				return PxResult::LvAttach(raw_stop(), true);
			}
			if (shouldRun()) {
				// This language makes me wanna kernel modesetting
				if (isRunning() == Exclude) return PxResult::LvAttach<bool>(start(), true);
			} else {
				if (isRunning() == Include) return PxResult::LvAttach<bool>(stop(), true);
			}
			return PxResult::Result(false);
		}
		PxResult::Result<void> reload() {
			auto parts = PxFunction::split(name, ":", 1);
			std::string baseName;

			if (parts.size() == 2) baseName = parts[0]+":";
			else if (parts.size() == 1) baseName = parts[0];
			else return PxResult::Result<void>("PxService::Service::reload", ENOENT);

			std::string filename;
			if (!isUser) {
				filename = "/var/lib/parallax-service/"+baseName;
				if (!std::filesystem::exists(filename)) {
					filename = "/usr/lib/parallax-service/"+baseName;
				}
				privileged = true;
			} else {
				char* home = std::getenv("HOME");

				if (home != NULL) {
					filename = (std::string)home + "/.local/lib/parallax-user-service/"+baseName;
					privileged = false;
				} else {
					filename = "";
				}
				if (!std::filesystem::exists(filename)) {
					privileged = true;
					filename = "/var/lib/parallax-user-service/"+baseName;
					if (!std::filesystem::exists(filename)) {
						filename = "/usr/lib/parallax-user-service/"+baseName;
					}
				}
			}
			if (!std::filesystem::exists(filename)) {
				return PxResult::Result<void>("PxService::Service::reload (no such service)", ENOENT);
			}
			auto res = PxConfig::ReadConfig(filename, parts.size()==2 ? parts[1] : "");
			PXASSERTM(res, "PxService::Service::reload");
			conf = res.assert();
			return PxResult::Null;
		}
	};
};

#endif
