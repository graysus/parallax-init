
#include <string>
#include <set>

#include <PxLog.hpp>
#include <PxResult.hpp>
#include <PxJob.hpp>
#include <PxIPCServ.hpp>

#ifndef PXSH
#define PXSH
namespace PxService {
	enum ServiceOverrideState {
		Include, Exclude, Auto
	};
	enum ServiceState {
		Inactive,   	// The service is not running
		Starting,   	// The service has been started, but has not finished
		Running,    	// The service has been started and is running
		Stopping,   	// The service was stopped by the user.
		Failed,	    	// The service crashed, failed to start or failed to stop.
		FailStopping,	// The service is stopping because it has failed
		Waiting,	// The service is waiting for a dependency to finish starting
		StopWaiting	// The service is waiting for its dependents to finish stopping
	};
	class LogServiceStartTask : public PxLog::LogTask {
	public:
		LogServiceStartTask(std::string svname) {
			me = svname;
			terse = "service "+svname;
		}
		std::string repr() override {
			switch (status) {
				case PxLog::Success:
					return "Started "+me;
				case PxLog::Partial:
					return "Cancelled starting "+me;
				case PxLog::Fail:
					return "Failed to start "+me;
				case PxLog::Pending:
					return "Starting "+me;
			}
			return me;
		}
	};
	class LogServiceStopTask : public PxLog::LogTask {
	public:
		LogServiceStopTask(std::string svname) {
			me = svname;
			terse = "stop-service "+svname;
		}
		std::string repr() override {
			switch (status) {
				case PxLog::Success:
					return "Stopped "+me;
				case PxLog::Partial:
					return "Cancelled stopping "+me;
				case PxLog::Fail:
					return "Failed to stop "+me;
				case PxLog::Pending:
					return "Stopping "+me;
			}
			return me;
		}
	};
	
	class Service;
	class Target;
	class ServiceManager;
	
	PxResult::Result<void> UpdateService(ServiceManager *mgr, std::string name);
	bool ReachedForTarget(ServiceManager *mgr, Target *tgt, std::string svcname);
	std::set<std::string> ServiceSet(ServiceManager *mgr);
	std::shared_ptr<Target> GetTargetOf(ServiceManager *mgr);
	PxJob::JobServer *GetJobsOf(ServiceManager *mgr);
	PxResult::Result<Service*> LoadServiceOf(ServiceManager *mgr, std::string name);
	bool CheckReach(ServiceManager *mgr);
	void ReadExportsOf(ServiceManager *mgr, std::vector<std::string> &strings);
	PxIPC::Server<char>* ServerOf(ServiceManager *mgr);
}
#endif
