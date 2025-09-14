#include <PxLog.hpp>
#include <PxJob.hpp>

using PxLog::log;

PxJob::JobServer jobs;
int t1;

void stuff() {
	log.info("This should be printed below the 'Hello World'");
}
void fizz() {
	log.warn("Fizz");
}
void buzz() {
	log.error("Buzz");
}
void finishT1() {
	log.completeTask(t1, PxLog::Success);
}

int main() {
	log.header("Hello world!");
	t1 = log.newTask(new PxLog::LogTask({"Task 1"}));
	log.newTask(new PxLog::LogTask({"Task 2"}));
	log.info("This should be printed below the 'Hello World'");

	jobs = PxJob::JobServer();

	jobs.AddJob(new PxJob::Interval(3000, fizz));
	jobs.AddJob(new PxJob::Timer(4000, stuff));
	jobs.AddJob(new PxJob::Interval(5000, buzz));
	jobs.AddJob(new PxJob::Timer(6000, finishT1));
	jobs.AddJob(new PxJob::OscJob(&PxLog::log));

	while (true) {
		usleep(10000);
		jobs.tick();
	}
}
