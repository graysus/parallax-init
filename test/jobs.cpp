#include <PxJob.hpp>
#include <PxFunction.hpp>
#include <ctime>
#include <iostream>

void fizz() {
	std::cout << "fizz\n";
}
void buzz() {
	std::cout << "buzz\n";
}

int main() {
	PxJob::JobServer jobs;
	jobs.AddJob(new PxJob::Interval(3000, fizz));
	jobs.AddJob(new PxJob::Interval(5000, buzz));
	while (true) {
		jobs.tick();
		usleep(10000); // 100 times/s
	}
}
