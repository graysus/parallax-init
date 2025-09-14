
#include <time.h>
#include <PxFunction.hpp>
#include <string>

#ifndef PXL
#define PXL

#define TIMEFMT "%Y-%m-%d-%H:%M:%S"

namespace PxLogging {
	struct LogEntry {
		time_t time;
		short ms;
		std::string cls;
		std::string message;
		std::string terse;

		std::string serialize() {
			return "time="+std::to_string(time)+",ms="+std::to_string(ms)+",class="+cls+",terse="+terse+",,,"+message;
		}
	};
	LogEntry ParseEntry(std::string line);
	LogEntry CreateEntry(std::string cls, std::string message, std::string terse = "not-implemented");
	std::string FormatTime(time_t intime);
	std::string FormatNow();
}

#endif
