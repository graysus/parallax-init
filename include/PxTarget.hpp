#include <PxServiceHelpers.hpp>
#include <PxProcess.hpp>
#include <PxState.hpp>
#include <PxResult.hpp>
#include <PxEnv.hpp>

#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/wait.h>
#include <filesystem>
#include <unistd.h>
#include <vector>

#ifndef PXSTARG
#define PXSTARG

namespace PxService {
	class Target {
	public:
		std::string name;
		std::set<std::string> attributes;
		std::string command;
		std::string colCommand;
		bool reachedTarget = false;
		ServiceManager *mgr;

		bool isUser;
		uid_t uid;

		Target(std::string Name, ServiceManager *Mgr, bool isuser, uid_t uid, bool rld = true) {
			name = Name;
			mgr = Mgr;
			isUser = isuser;
			this->uid = uid;
			if (rld) reload();
		}
		virtual void reload() {
			for (auto &i : getSearchLocs()) {
				if (std::filesystem::exists(i+"/attribute")) {
					for (auto &m : std::filesystem::directory_iterator(i+"/attribute")) {
						attributes.insert(m.path().filename());
					}
				}
			}
			for (auto &i : getSearchLocs()) {
				if (std::filesystem::exists(i+"/command")) {
					auto res = PxState::fget(i+"/command");
					if (!res.eno) command = res.assert();
				}
				if (std::filesystem::exists(i+"/colCommand")) {
					auto res = PxState::fget(i+"/colCommand");
					if (!res.eno) colCommand = res.assert();
				}
			}
		}
		virtual bool HasAttribute(std::string attribute) {
			return attributes.count(attribute) > 0;
		}
		virtual bool Disabled(std::string servname) {
			return !search("service-disable/"+servname).empty();
		}
		virtual bool Enabled(std::string servname) {
			return !search("service-enable/"+servname).empty();
		}

		std::string search(std::string filename) {
			for (auto &i : getSearchLocs()) {
				if (std::filesystem::exists(i+"/"+filename)) {
					return i+"/"+filename;
				}
			}
			return "";
		}

		std::vector<std::string> getSearchLocs(bool includeGlobal = true) {
			if (isUser) {
				char* rawhome = std::getenv("HOME");
				std::string home = rawhome == NULL ? "/root" : rawhome;

				if (includeGlobal)
					return {
						home+"/.local/lib/parallax-user-target/"+name,
						"/var/lib/parallax-user-target/"+name,
						"/usr/lib/parallax-user-target/"+name,
					};
				else
					return {
						home+"/.local/lib/parallax-user-target/"+name
					};
			} else {
				return {
					"/var/lib/parallax-target/"+name,
					"/usr/lib/parallax-target/"+name,
				};
			}

		}

		virtual std::set<std::string> List() {
			// /usr/lib/parallax-target/default/service-enable/svc1
			std::vector<std::string> searchLocs;
			std::set<std::string> result;

			for (auto &i : getSearchLocs()) {
				searchLocs.push_back(i+"/service-enable");
				searchLocs.push_back(i+"/service-disable");
			}

			for (auto &i : searchLocs) {
				if (std::filesystem::exists(i)) {
					for (auto &m : std::filesystem::directory_iterator(i)) {
						result.insert(m.path().filename());
					}
				}
			}

			result.merge(ServiceSet(mgr));
			
			return result;
		}
		virtual bool hasReached() {
			for (auto &i : List()) {
				if (!ReachedForTarget(mgr, this, i)) {
					return false;
				}
			}
			return true;
		}
		virtual void onReach() {
			std::vector<std::string> environment;
			ReadExportsOf(mgr, environment);

			if (!command.empty()) {
				if (PxFunction::trim(command) == "exit") {
					exit(0);
				}
				int f = fork();
				if (f<0) {
					PxLog::log.error((std::string)"Failed to execute target command: "+strerror(errno));
				} else if (!f) {
					PxProcess::CloseFD();
					PxFunction::wrap("setuid", setuid(uid)).assert("onReach");
					for (auto &i : environment) {
						putenv((char*)i.c_str());
					}
					execl("/bin/sh", "sh", "-c", ("exec "+command).c_str(), NULL);
					exit(1);
				} else {
					int wres;
					waitpid(f, &wres, 0);
					if (WEXITSTATUS(wres) > 0) {
						PxLog::log.error("Command returned with exit code "+std::to_string(WEXITSTATUS(wres)));
					}
				}
			}
		}
		virtual void onCollectiveReach() {
			PxFunction::wrap("seteuid", seteuid(0)).assert("PxService::Target::onCollectiveReach");
			PxFunction::mkdirs("/run/targetenv").assert();
			std::string path;
			if (isUser) {
				path = "/run/targetenv/all-u"+std::to_string(uid);
			} else {
				path = "/run/targetenv/all";
			}
			std::vector<std::string> environment;
			ReadExportsOf(mgr, environment);
			PxEnv::dumpStrings(path, environment);

			PxFunction::wrap("seteuid", seteuid(uid)).assert("PxService::Target::onCollectiveReach");

			if (!colCommand.empty()) {
				if (PxFunction::trim(colCommand) == "exit") {
					exit(0);
				}
				int f = fork();
				if (f<0) {
					PxLog::log.error((std::string)"Failed to execute target command: "+strerror(errno));
				} else if (!f) {
					PxProcess::CloseFD();
					PxFunction::wrap("setuid", setuid(uid)).assert("onCollectiveReach");
					for (auto &i : environment) {
						putenv((char*)i.c_str());
					}
					execl("/bin/sh", "sh", "-c", ("exec "+colCommand).c_str(), NULL);
					exit(1);
				} else {
					int wres;
					waitpid(f, &wres, 0);
					if (WEXITSTATUS(wres) > 0) {
						PxLog::log.error("Command returned with exit code "+std::to_string(WEXITSTATUS(wres)));
					}
				}
			}
		}
	};

	class CollectiveTarget : public Target {
	public:
		std::vector<std::shared_ptr<Target>> tgts;
		CollectiveTarget(ServiceManager *Mgr, bool isUser, uid_t uid) : Target("collective", Mgr, isUser, uid, false) {
			// ...
		}
		void AddTarget(std::shared_ptr<Target> tgt) {
			if (tgt->HasAttribute("base")) {
				tgts = {};
			}
			tgts.push_back(tgt);
		}
		void RemoveTarget(std::shared_ptr<Target> tgt) {
			PxFunction::vecRemoveItem(&tgts, tgt);
		}
		void reload() override {
			for (auto &i : tgts) i->reload();
		}
		bool HasAttribute(std::string attribute) override {
			for (auto &i : tgts) {
				if (i->HasAttribute(attribute)) return true;
			}
			return false;
		}
		bool Disabled(std::string servname) override {
			bool isDisabled = false;
			for (auto &i : tgts) {
				if (i->Enabled(servname)) isDisabled = false;
				if (i->Disabled(servname)) isDisabled = true;
			}
			return isDisabled;
		}
		bool Enabled(std::string servname) override {
			bool isEnabled = false;
			for (auto &i : tgts) {
				if (i->Disabled(servname)) isEnabled = false;
				if (i->Enabled(servname)) isEnabled = true;
			}
			return isEnabled;
		}
		bool hasReached() override {
			for (auto &i : tgts) {
				if (!i->hasReached()) return false;
			}
			return true;
		}
		std::set<std::string> List() override {
			std::set<std::string> list;
			for (auto &i : tgts) {
				list.merge(i->List());
			}
			return list;
		}
		void onReach() override {
			onCollectiveReach();
		}
		void onCollectiveReach() override {
			for (auto &i : tgts) {
				i->onCollectiveReach();
			}
		}

	};
}

#endif
