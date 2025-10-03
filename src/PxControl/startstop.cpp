#include <PxResult.hpp>
#include <vector>
#include <PxIPC.hpp>
#include <PxControl.hpp>

PxResult::Result<void> CmdStart(std::vector<std::string> args) {
	PxIPC::Client cli;
	PXASSERTM(cli.Connect(getsvman()), "PxControl / CmdStart");
	for (auto &i : args) {
		PXASSERTM(cli.Write("start "+i+'\0'), "PxControl / CmdStart");

		auto res = cli.Read();
		PXASSERTM(res, "PxControl / CmdStart");
		auto stat = res.assert();

		auto splitted = PxFunction::split(stat);
		if (splitted.size() < 3 || splitted[0] == "error" || splitted[2] != "started") {
			PxLog::log.error("Failed to start "+i+", see logs for details.");
			return PxResult::Null;
		}
	}

	if (args.size() == 1) {
		PxLog::log.info("Started service "+args[0]+".");
	} else {
		PxLog::log.info("Started "+std::to_string(args.size())+" services.");
	}
	return PxResult::Null;
}

PxResult::Result<void> CmdStop(std::vector<std::string> args) {
	PxIPC::Client cli;
	auto res = cli.Connect(getsvman());
	if (res.eno) return res;
	for (auto &i : args) {
		PXASSERTM(cli.Write("stop "+i+'\0'), "PxControl / CmdStop");

		auto res = cli.Read();
		PXASSERTM(res, "PxControl / CmdStop");
		auto stat = res.assert();

		auto splitted = PxFunction::split(stat);
		if (splitted.size() < 3 || splitted[0] == "error" || splitted[2] != "stopped") {
			PxLog::log.error("Failed to stop "+i+", see logs for details.");
			return PxResult::Null;
		}
	}
	if (args.size() == 1) {
		PxLog::log.info("Stopped service "+args[0]+".");
	} else {
		PxLog::log.info("Stopped "+std::to_string(args.size())+" services.");
	}
	return PxResult::Null;
}

PxResult::Result<void> CmdRestart(std::vector<std::string> args) {
	PxIPC::Client cli;
	PXASSERTM(cli.Connect(getsvman()), "PxControl / CmdStart");
	for (auto &i : args) {
		{
            PXASSERTM(cli.Write("stop "+i+'\0'), "PxControl / CmdRestart");

            auto res = cli.Read();
            PXASSERTM(res, "PxControl / CmdRestart");
            auto stat = res.assert();

            auto splitted = PxFunction::split(stat);
            if (splitted.size() < 3 || splitted[0] == "error" || splitted[2] != "stopped") {
                PxLog::log.error("Failed to stop "+i+", see logs for details.");
                return PxResult::Null;
            }
        }
        {
            PXASSERTM(cli.Write("start "+i+'\0'), "PxControl / CmdRestart");

            auto res = cli.Read();
            PXASSERTM(res, "PxControl / CmdRestart");
            auto stat = res.assert();

            auto splitted = PxFunction::split(stat);
            if (splitted.size() < 3 || splitted[0] == "error" || splitted[2] != "started") {
                PxLog::log.error("Failed to start "+i+", see logs for details.");
                return PxResult::Null;
            }
        }
	}

	if (args.size() == 1) {
		PxLog::log.info("Restarted service "+args[0]+".");
	} else {
		PxLog::log.info("Restarted "+std::to_string(args.size())+" services.");
	}
	return PxResult::Null;
}