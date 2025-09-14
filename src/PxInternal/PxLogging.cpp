
#include <PxLogging.hpp>
#include <map>

namespace PxLogging {
	LogEntry ParseEntry(std::string line) {
		LogEntry entry;

		auto spl = PxFunction::split(line, ",,,", 1);
		if (spl.size() < 2) spl.push_back("");

		std::map<std::string, std::string> props;
		for (auto &i : PxFunction::split(spl[0], ",")) {
			auto prop = PxFunction::split(i, "=", 1);
			if (prop.size() < 2) continue;
			props[prop[0]] = prop[1];
		}

		entry.time = std::atoi(props["time"].c_str());
		entry.ms = std::atoi(props["ms"].c_str());
		entry.cls = props["class"];
		entry.terse = props["terse"];
		entry.message = spl[1];
		return entry;
	}
	LogEntry CreateEntry(std::string cls, std::string message, std::string terse) {
		LogEntry entry;
		entry.time = time(NULL);
		entry.ms = PxFunction::now()%1000;
		entry.cls = cls;
		entry.message = message;
		entry.terse = terse;
		return entry;
	}
	std::string FormatTime(time_t intime) {
		auto lt = localtime(&intime);
		char buf[128];
		strftime(buf, 128, TIMEFMT, lt);
		return std::string(buf);
	}
	std::string FormatNow() {
		return FormatTime(time(NULL));
	}
}
