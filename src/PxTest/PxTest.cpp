
#include <PxProcess.hpp>
#include <PxIPCServ.hpp>
#include <PxIPC.hpp>
#include <PxArg.hpp>
#include <PxState.hpp>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <vector>
#include <sys/wait.h>
#include <map>


struct PxTest {
	std::string name;
	std::string id;
	std::string prog;
	std::string cmdline = "";
	std::string expect;
};

std::vector<PxTest> tests = {
	{"Long arguments", "args-long", "args", "--long-argument", "AL;"},
	{"Short arguments", "args-short", "args", "-abc", "A1;A2;A3;"},
	{"Short arguments mixed with long arguments", "args-mixed", "args", "-ac --long-argument -b --long-argument-2", "A1;A2;A3;AL;AL2;"},
	{"Arguments with value", "args-value", "args", "--argument-y value-y -xvalue-x -z value-z", "AVX;value-x;AVY;value-y;AVZ;value-z;"},
	{"Configuration file with parameters", "config1", "config", "testsuite/ConfTest1 Val1:Val2:Val3", "VAR_Param1;Val1;VAR_Param2;Val2:Val3;VAR_ParamAll;Val1:Val2:Val3;"},
	{"Configuration file with backslash escapes", "config2", "config", "testsuite/ConfTest2 x", "VAR_EscapeLF;Line Feed \\n escape;VAR_EscapeSlash;Backslash \\\\ escape;VAR_EscapeDollar;Dollar $ escape;"},
	{"String manipulation (split+join)", "split-join", "string", "split-join", "Value First;Value Second;Value Third;Value Fourth;"},
	{"String manipulation (trim)", "trim", "string", "trim", "much whitespace\\nleft whitespace\\nno whitespace\\nend"},
};
// args-value=AVX;value-x;AVY;value-y;AVZ;value-z;

std::map<std::string, std::string> outputs;

PxResult::Result<void> OnCommand(PxIPC::EventContext<char> *ctx) {
	auto splcmd = PxFunction::split(ctx->command, " ");
	if (splcmd[0] == "report") {
		auto reportOn = splcmd[1];
		PxFunction::vecRemoveIndeces(&splcmd, 0, 2);
		outputs[reportOn] = PxFunction::join(splcmd, " ");
	} else if (splcmd[0] == "terminate") {
		if (outputs.count(splcmd[1]) == 0) {
			outputs[splcmd[1]] = "<failed with no response>";
		}
	} else if (splcmd[0] == "write-to") {
		std::string out;
		for (auto &i : outputs) {
			out += i.first + "=" + i.second + "\n";
		}
		PxState::fput(splcmd[1], out).assert();
	} else if (splcmd[0] == "finish") {
		exit(0);
	}
	return PxResult::Null;
}

int RunServer() {
	PxProcess::CloseFD();
	PxIPC::Server<char> serv;
	serv.Init("/tmp/pxtest.sock").assert();
	serv.on_command = OnCommand;

	while (true) {
		usleep(1000);
		serv.Update().assert();
	}

	return 0;
}

int main(int argc, const char *argv[]) {
	auto args = PxFunction::vectorize(argc, argv);

	PxArg::Argument output("output", 'o');
	PxArg::Argument nobuild("nobuild", 'n');

	PxArg::ArgParser parser({}, {&output, &nobuild});
	auto res = parser.parseArgs(args);
	auto positional = res.assert("Test suite");

	if (!nobuild.active) {
		PxLog::log.info("Clean building...");
		if (system("make -C testsuite clean") != 0) {
			PxLog::log.error("Clean building was unsuccessful. (while cleaning)");
			return 1;
		}
		if (system("make -C testsuite all") != 0) {
			PxLog::log.error("Clean building was unsuccessful. (while making)");
			return 1;
		}
	}

	// Run the server
	int f = fork();
	if (f < 0) {
		perror("fork");
		return 1;
	}
	if (!f) return RunServer();

	std::vector<int> pids;
	
	for (auto &i : tests) {
		int f2 = fork();
		if (!f2) {
			//PxProcess::CloseFD();
			std::vector<std::string> vec = {"testsuite/"+i.prog+".tstexc", i.id};

			for (auto &i : PxFunction::split(i.cmdline, " ")) {
				vec.push_back(i);
			}

			char **args = (char**)malloc(sizeof(char *const)*(vec.size()+1));

			for (int i = 0; i < vec.size(); i++) {
				args[i] = (char*)vec[i].c_str();
			}
			args[vec.size()] = NULL;
			if (execv(vec[0].c_str(), (char *const *)args) < 0) {
				perror("execv");
			}
			
			free(args);
			exit(1);
		}
		pids.push_back(f2);
	}
	for (auto &i : pids) {
		int wres;
		if (waitpid(i, &wres, 0) < 0) {
			perror("waitpid");
			return 1;
		}
		int exitcode = WEXITSTATUS(wres); // TODO: use exit code
		if (exitcode != 0) {
			PxLog::log.error("Error code "+std::to_string(exitcode));
		}
	}

	remove("/tmp/results.pipe");
	mkfifo("/tmp/results.pipe", 0600);
	
	{
		PxIPC::Client cli;
		cli.Connect("/tmp/pxtest.sock").assert();
		cli.Write((std::string)"write-to /tmp/results.pipe"+'\0'+"finish"+'\0').assert();
	}
	
	PxLog::log.info("Getting results...");
	auto res2 = PxState::fgetp("/tmp/results.pipe");
	auto results_split = PxFunction::split(res2.assert("PxTest"), "\n");
	for (auto &i : results_split) {
		if (i.length() == 0) continue;
		auto kv = PxFunction::split(i, "=", 1);
		if (kv.size() < 2) {
			PxLog::log.error("Expected expression: "+i);
		}
		outputs[kv[0]] = kv[1];
	}

	int success = 0;
	int fail = 0;
	
	for (auto &i : tests) {
		if (outputs[i.id] == i.expect) {
			success++;
		} else {
			PxLog::log.error("Test "+i.name+" failed.");
			PxLog::log.info("Expected value: "+i.expect);
			PxLog::log.info("Actual value: "+outputs[i.id]);
			fail++;
		}
	}
	PxLog::log.header("Number of successes: "+std::to_string(success));
	PxLog::log.header("Number of failures: "+std::to_string(fail));
}
