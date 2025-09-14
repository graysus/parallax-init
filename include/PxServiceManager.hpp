#include <PxServiceHelpers.hpp>
#include <PxTarget.hpp>
#include <PxService.hpp>
#include <PxIPCServ.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#ifndef PXSMGR
#define PXSMGR

namespace PxService {
	class ServiceManager {
	public:
		std::shared_ptr<ServiceManager>* me;
		std::shared_ptr<CollectiveTarget> tgt;
		PxJob::JobServer *jobs;
		std::map<std::string, std::shared_ptr<Service>> services;
		bool reachedTarget;
		std::map<std::string, std::string> exports;

		bool isUser = false;
		uid_t uid = 0;
		PxIPC::Server<char>* server;
	
		ServiceManager() {}

		void init(bool isuser, uid_t uid) {
			tgt = std::make_shared<CollectiveTarget>(this, isUser, uid);
			isUser = isuser;
			this->uid = uid;
		}
		virtual ~ServiceManager() {
			for (auto &i : services) {
				i.second->mgr = NULL;
			}
		}
		PxResult::Result<Service*> ReloadService(std::string ServiceName) {
			bool justCreated = false;
			if (services[ServiceName] == nullptr) {
				services[ServiceName] = std::make_shared<Service>(ServiceName, this, isUser, uid);
				justCreated = true;
			}
			if (services[ServiceName]->isRunning() == Exclude) {
				auto result = services[ServiceName]->reload();
				if (result.eno) {
					if (justCreated) {
						services[ServiceName] = nullptr;
					}
					return PxResult::FResult(result).merge("PxService::ReloadService");
				}
			}
			return services[ServiceName].get();
		}
		PxResult::Result<void> StartService(std::string ServiceName, size_t hndl = SIZE_MAX) {
			// Start request from the user or a program.
			// Note: This should only be called by the user or
			//       a regular program. Services should depend
			//       on the service instead.
			PxResult::Result<Service*> serviceres = ReloadService(ServiceName);
			if (serviceres.eno) return PxResult::Clear(serviceres).merge("PxService::StartService");
			auto service = serviceres.assert();

			if (hndl != SIZE_MAX) service->subscribed_handles.push_back(hndl);
			
			service->failLocked = false;
			service->userOverride = ServiceOverrideState::Include;
			return PxResult::LvClear(service->update()).merge("PxService::StartService");
		}
		PxResult::Result<void> StopService(std::string ServiceName, size_t hndl = SIZE_MAX) {
			// Stop request from the user or a program.
			// Note: This should only be called by the user or
			//       a regular program. Services should depend
			//       on the service instead.
			PxResult::Result<Service*> serviceres = ReloadService(ServiceName);
			if (serviceres.eno) return PxResult::Clear(serviceres).merge("PxService::StopService");
			auto service = serviceres.assert();
			
			if (hndl != SIZE_MAX) service->subscribed_handles.push_back(hndl);
			
			service->failLocked = false;
			if (service->status == ServiceState::Failed) {
				service->status = ServiceState::Inactive;
			}
			service->userOverride = ServiceOverrideState::Exclude;
			return PxResult::LvClear(service->update()).merge("PxService::StopService");
		}

		PxResult::Result<void> FailService(std::string ServiceName) {
			// Service is failing
			PxResult::Result<Service*> serviceres = ReloadService(ServiceName);
			if (serviceres.eno) return PxResult::Clear(serviceres).merge("PxService::StopService");
			auto service = serviceres.assert();
			
			service->failLocked = true;
			return PxResult::LvClear(service->update()).merge("PxService::StopService");
		}
		PxResult::Result<void> CascadeStopService(std::string ServiceName) {
			// Cascading stop request from the user or a program.
			// Note: This should only be called by the user or
			//       a regular program. Services should depend
			//       on the service instead.
			PxResult::Result<Service*> serviceres = ReloadService(ServiceName);
			if (serviceres.eno) return PxResult::Clear(serviceres).merge("PxService::CascadeStopService");
			auto service = serviceres.assert();
			
			service->failLocked = false;
			if (service->status == ServiceState::Failed) {
				service->status = ServiceState::Inactive;
			}
			return service->cascadeStop().merge("PxService::CascadeStopService");
		} 
		PxResult::Result<void> FinishStartService(std::string ServiceName) {
			// Finish start request from the service.
			// Note: This should only be called by the service
			PxResult::Result<Service*> serviceres = ReloadService(ServiceName);
			if (serviceres.eno) return PxResult::Clear(serviceres).merge("PxService::FinishStartService");
			auto service = serviceres.assert();
			
			service->finishStart();
			return PxResult::Null;
		}
		PxResult::Result<void> FinishStopService(std::string ServiceName) {
			// Finish stop request from the service.
			// Note: This should only be called by the service
			PxResult::Result<Service*> serviceres = ReloadService(ServiceName);
			if (serviceres.eno) return PxResult::Clear(serviceres).merge("PxService::FinishStopService");
			auto service = serviceres.assert();
			
			service->finishStop();
			return PxResult::Null;
		} 
		PxResult::Result<void> setTarget(std::string TargetName) {
			for (auto& i : tgt->tgts) {
				if (i->name == TargetName) return PxResult::Null;
			}
			auto curtgt = std::make_shared<Target>(TargetName, this, isUser, uid);
			tgt->AddTarget(curtgt);
			if (curtgt->HasAttribute("system-shutdown")) {
				auto res = PxFunction::chvt(1);
				if (res.eno != 0) return res.merge("PxServiceManager::setTarget");
			}
			reachedTarget = false;
			auto servList = tgt->List();
			for (auto &i : servList) {
				auto res = ReloadService(i);
				if (res.eno == ENOENT) continue;
				else if (res.eno) {
					PxLog::log.error("Error loading service "+i+": "+res.funcName+": "+strerror(res.eno));
					continue;
				}
				res.assert();
			}
			for (auto &i : servList) {
				auto res1 = ReloadService(i);
				if (res1.eno) {
					PxLog::log.error("Error updating service "+i+": "+res1.funcName+": "+strerror(res1.eno));
					continue;
				}
				auto res2 = res1.assert()->update();
				if (res2.eno) {
					PxLog::log.error("Error updating service "+i+": "+res2.funcName+": "+strerror(res2.eno));
					continue;
				}
			}
			ReachedTarget();
			return PxResult::Null;
		}
		bool ReachedTarget() {
			int reachedTargets = 0;
			for (auto &i : tgt->tgts) {
				if (!i->hasReached()) continue;
				if (!i->reachedTarget) {
					PxLog::log.info("Reached target "+i->name, "reached-target "+i->name);
					i->onReach();
				}
				reachedTargets++;
				i->reachedTarget = true;
			}
			if (reachedTargets < tgt->tgts.size()) return false;
			if (!reachedTarget) {
				PxLog::log.info("Reached all targets", "reached-all-targets");
				tgt->onReach();
			}
			reachedTarget = true;
			return true;
		}
	};
};
#endif
