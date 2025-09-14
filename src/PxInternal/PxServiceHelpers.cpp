#include <string>
#include <PxServiceHelpers.hpp>
#include <PxTarget.hpp>
#include <PxService.hpp>
#include <PxServiceManager.hpp>
#include <vector>

namespace PxService {
	std::shared_ptr<Target> GetTargetOf(ServiceManager *mgr) {
		return mgr->tgt;
	}
	
	PxJob::JobServer *GetJobsOf(ServiceManager *mgr) {
		return mgr->jobs;
	}

	PxResult::Result<Service*> LoadServiceOf(ServiceManager *mgr, std::string name) {
		return mgr->ReloadService(name);
	}
	PxResult::Result<void> UpdateService(ServiceManager *mgr, std::string name) {
		auto res = mgr->ReloadService(name);
		PXASSERTM(res, "PxService::UpdateService");
		return res.assert()->update().merge("PxService::UpdateService");
	}
	bool CheckReach(ServiceManager *mgr) {
		return mgr->ReachedTarget();
	}
	bool ReachedForTarget(ServiceManager *mgr, Target *tgt, std::string svcname) {
		auto res = mgr->ReloadService(svcname);
		if (res.eno) return true;  // TODO: investigate possible issues with this solution
		auto svc = res.assert();
		// bool shouldRun = svc->shouldRun();
		if (tgt->HasAttribute("system-shutdown") && svc->getProperty("IgnoreShutdown", "false") != "true") {
			return svc->isRunning() == Exclude;
		}
		//if (shouldRun && tgt->Disabled(svcname)) return false;
		//if (!shouldRun && tgt->Enabled(svcname)) return false;
		return svc->isRunning() != Auto;
	}
	std::set<std::string> ServiceSet(ServiceManager *mgr) {
		std::set<std::string> result;
		for (auto &i : mgr->services) {
			result.insert(i.first);
		}
		return result;
	}
	void ReadExportsOf(ServiceManager *mgr, std::vector<std::string> &strings) {
		strings = std::vector<std::string>();
		strings.reserve(mgr->exports.size());
		for (auto &i : mgr->exports) {
			strings.push_back(i.second);
		}
	}
	PxIPC::Server<char>* ServerOf(ServiceManager *mgr) {
		return mgr->server;
	}
}
