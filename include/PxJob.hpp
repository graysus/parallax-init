
#include <PxResult.hpp>
#include <PxFunction.hpp>
#include <string>
#include <vector>
#include <cstring>
#include <PxLog.hpp>

#ifndef PXJOB
#define PXJOB

namespace PxJob {
	typedef void(*VoidCallback)();
	typedef PxResult::Result<void>(*VResCallback)();
	class JobServer;
	class Job {
	public:
		JobServer* serv;
		bool started = false;
		bool finished = false;
		bool failed = false;
		bool finishing = false;
		Job() {}
		virtual PxResult::Result<void> start() {
			return PxResult::Null;
		}
		virtual PxResult::Result<void> tick() {
			return PxResult::Null;
		}
		virtual PxResult::Result<void> finish() {
			return PxResult::Null;
		}
		virtual std::string tell() {
			return "Generic job";
		}
		virtual void onerror(PxResult::Result<void> err) {
			failed = true;
			finished = true;
			errno = err.eno;
			std::string es = "Ignoring error in job "+tell()+": "+err.funcName+strerror(errno);
			PxLog::log.error(es);
		}
	};
	class JobServer {
	public:
		std::vector<std::shared_ptr<Job>> jobs;
		JobServer() {}
		template<typename T> void AddJob(std::shared_ptr<T> job) {
			job->serv = this;
			jobs.push_back(job);
		}
		void tick() {
			for (int i = 0; i < jobs.size(); i++) {
				auto j = jobs[i];
				if (j.get() == NULL) exit(1);
				PxResult::Result<void> res;
				if (j->finished) {
					jobs.erase(std::next(jobs.begin(), i), std::next(jobs.begin(), i+1));
					// For some reason, i-- causes i to go into the negatives
					// ...even though it immediately gets incremented. Why?
					// It doesn't matter anyway, erase it on the next iteration.
					continue;
				}
				if (!j->started) res = j->start();
				if (res.eno) {
					j->onerror(res);
					continue;
				}
				j->started = true;
				res = j->tick();
				if (res.eno) {
					j->onerror(res);
					continue;
				}
				if (!j->finishing) continue;
				res = j->finish();
				if (res.eno) {
					j->onerror(res);
					continue;
				}
				j->finished = true;
			}
		}
	};
	class Interval : public PxJob::Job {
	public:
		VoidCallback cb;
		int lastUpdate;
		int dur;
		Interval(float dur, VoidCallback cb) {
			this->cb = cb;
			this->dur = dur;
			this->lastUpdate = PxFunction::now();
		}
		PxResult::Result<void> tick() override {
			if (PxFunction::now() - lastUpdate > dur) {
				cb();
				lastUpdate = PxFunction::now();
			}
			return PxResult::Null;
		}
		std::string tell() override {
			return "Interval";
		}
	};
	class Timer : public PxJob::Job {
	public:
		VoidCallback cb;
		int lastUpdate;
		int dur;
		Timer(float dur, VoidCallback cb) {
			this->cb = cb;
			this->dur = dur;
			this->lastUpdate = PxFunction::now();
		}
		PxResult::Result<void> tick() override {
			if (PxFunction::now() - lastUpdate > dur) {
				cb();
				finishing = true;
			}
			return PxResult::Null;
		}
		std::string tell() override {
			return "Timer";
		}
	};
	class RsInterval : public PxJob::Job {
	public:
		VResCallback cb;
		int lastUpdate;
		int dur;
		RsInterval(float dur, VResCallback cb) {
			this->cb = cb;
			this->dur = dur;
			this->lastUpdate = PxFunction::now();
		}
		PxResult::Result<void> tick() override {
			PxResult::Result<void> res;
			if (PxFunction::now() - lastUpdate > dur) {
				res = cb();
				lastUpdate = PxFunction::now();
			}
			return res;
		}
		std::string tell() override {
			return "Interval";
		}
	};
	class RsTimer : public PxJob::Job {
	public:
		VResCallback cb;
		int lastUpdate;
		int dur;
		RsTimer(float dur, VResCallback cb) {
			this->cb = cb;
			this->dur = dur;
			this->lastUpdate = PxFunction::now();
		}
		PxResult::Result<void> tick() override {
			PxResult::Result<void> res;
			if (PxFunction::now() - lastUpdate > dur) {
				res = cb();
				finishing = true;
			}
			return res;
		}
		std::string tell() override {
			return "Timer";
		}
	};
	class OscJob : public PxJob::Job {
	public:
		PxLog::Log *myLog;
		time_t msStartTime;
		OscJob(PxLog::Log *MyLog) {
			myLog = MyLog;
		}
		PxResult::Result<void> start() override {
			msStartTime = PxFunction::now();
			return PxResult::Null;
		}
		PxResult::Result<void> tick() override {
			if (PxFunction::now() - msStartTime >= 500) {
				msStartTime = PxFunction::now();
				myLog->oscillation = !myLog->oscillation;
				myLog->updateTasks();
			}
			return PxResult::Null;
		}
		std::string tell() override {
			return "OscJob";
		}
	};
}
#endif
