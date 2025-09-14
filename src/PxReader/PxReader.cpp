#include <PxFunction.hpp>
#include <PxLog.hpp>
#include <PxLogging.hpp>
#include <PxResult.hpp>
#include <PxState.hpp>
#include <PxArg.hpp>
#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <config.hpp>

std::string showTime(time_t second, time_t first_time, short ms, bool terseout, bool relative) {
	std::string shown_time;
	size_t timestr_size;

	if (relative) {
		time_t reltime = (second * 1000 + ms) - first_time;
		shown_time = std::to_string(reltime)+"ms";
		// good for ~1157 day uptime
		timestr_size = 20;
	} else {
		shown_time = PxLogging::FormatTime(second)+"."+(std::to_string(ms+1000).substr(1, 3));
		timestr_size = 31;
	}

	if (terseout) return shown_time;

	auto timeStr = "\x1b[90m["+shown_time+"]";

	while (timeStr.length() < timestr_size) {
		if (relative)
			timeStr = ' ' + timeStr;
		else
			timeStr += ' ';
	}

	return timeStr;
}

void cmdRead(const std::vector<std::string>& lines, std::string fromUnit, bool relative, bool showterse, bool showterseout) {
	time_t first_time = 0;
	for (auto &i : lines) {
		if (i.length() == 0) continue;
		auto line = PxLogging::ParseEntry(i);
		if (first_time == 0) first_time = line.time * 1000 + line.ms;
		
		if (fromUnit != "" && line.cls != fromUnit) {
			continue;
		}

		std::string shown_time = showTime(line.time, first_time, line.ms, showterseout, relative);
		auto shown_message = showterse ? line.terse : line.message;
		if (showterseout) {
			PxLog::log.println(shown_time+"\t"+line.cls+"\t"+shown_message);
		} else {
			auto servStr = "\x1b[94m[from "+line.cls+"]";
			PxLog::log.println(shown_time+" "+servStr+" \x1b[0m"+shown_message);
		}
	}
}

void cmdAnalyze(const std::vector<std::string>& lines, bool relative, bool showterse, bool showterseout) {
	time_t first_time = 0;
	std::map<std::string, unsigned int> start_times;
	for (auto &i : lines) {
		if (i.length() == 0) continue;
		auto line = PxLogging::ParseEntry(i);
		if (first_time == 0) first_time = line.time * 1000 + line.ms;

		if (line.cls != "daemon") {
			continue;
		}

		auto splitted = PxFunction::split(line.terse);

		if (splitted.size() == 0) continue;
		std::string shown_message;

		if (splitted[0] == "reached-target") {
			shown_message = "  -> Target "+splitted[1];
		} else if (splitted.size() >= 3 && splitted[1] == "service") {
			if (splitted[0] == "success") {
				shown_message = "    -> Service "+splitted[2];
				if (start_times.count("start "+splitted[2]) != 0) {
					unsigned int ms = (line.time * 1000 + line.ms) - start_times["start "+splitted[2]]; 
					shown_message += " \x1b[90m(" + std::to_string(ms) + "ms to start)";
				} else {
					shown_message += " \x1b[90m(unknown)";
				}
			} else if (splitted[0] == "newtask" || splitted[0] == "starting") {
				start_times["start "+splitted[2]] = line.time * 1000 + line.ms;
			}
		}

		if (shown_message == "") {
			continue;
		}

		std::string shown_time = showTime(line.time, first_time, line.ms, showterseout, relative);

		if (showterseout) {
			PxLog::log.println(shown_time+"\t"+shown_message);
		} else {
			PxLog::log.println(shown_time+" \x1b[0m"+shown_message);
		}
	}
}

int main(int argc, const char* argv[]) {
	auto args = PxFunction::vectorize(argc, argv);
	
	PxArg::SelectArgument Command("COMMAND", "Which command to run", true);

	Command.addOption("read", "Read the contents of the log files [default]");
	Command.addOption("analyze", "Analyze the system init process");
	Command.addOption("version", "Show version information");
	Command.addOption("license", "Show the license of pxread");

	PxArg::Argument HelpArg("help", 'h', "Show this message");
	PxArg::Argument ShowTerse("terse-info", 't', "Show terse versions of messages");
	PxArg::Argument ShowTerseOutput("terse-output", 'T', "Show terse output");
	PxArg::Argument RelativeArg("boot-time", 'b', "Show milliseconds from first message instead of date and time");
	PxArg::ValueArgument LogFile("logfile", 'l', "Log file to read", true);
	PxArg::ValueArgument FromUnit("from", 'f', "Only show messages from a given service");
	
	PxArg::ArgParser parser({&Command}, {&HelpArg, &RelativeArg, &FromUnit, &LogFile, &ShowTerse, &ShowTerseOutput});
	parser.cmdline = argv[0];
	parser.description = "Read the Parallax logs";
	auto res = parser.parseArgs(args);
	
	if (HelpArg.active) {
		parser.printHelp();
		return 0;
	}
	if (res.eno == EINVAL) {
		PxLog::log.error("Bad argument.");
		return 1;
	}
	std::string logfile = LogFile.active ? LogFile.value : "current";

	auto res2 = PxState::fget("/var/log/parallax/"+logfile);
	if (res2.eno == ENOENT) {
		PxLog::log.error("No such log file "+logfile);
		return 1;
	} else if (res2.eno) {
		PxLog::log.error(res2.funcName + ": " + strerror(res2.eno));
		return 1;
	}
	
	auto logContent = res2.assert();
	auto lines = PxFunction::split(logContent, "\n");

	if (!Command.active || Command.value == "read") {
		cmdRead(lines, FromUnit.value, RelativeArg.active, ShowTerse.active, ShowTerseOutput.active);
	} else if (Command.value == "analyze") {
		cmdAnalyze(lines, RelativeArg.active, ShowTerse.active, ShowTerseOutput.active);
	} else if (Command.value == "version") {
		PxLog::log.info("Parallax version " PXVER);
		PxLog::log.info("pxread is a part of the parallax-init project.");
		PxLog::log.info("parallax-init is free software; you may redistribute it under the terms of the MIT License.");
		PxLog::log.info("Type \"pxread license\" for more information.");
	} else if (Command.value == "license") {
		PxLog::log.println(PXLICENSE);
	}
}