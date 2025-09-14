#include <PxIPC.hpp>
#include <PxJob.hpp>
#include <PxIPCServ.hpp>
#include <PxLogging.hpp>

#include <cerrno>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define PXL_BUF_SIZE 1024

PxIPC::Server<char> serv;
PxJob::JobServer jobs;
FILE *LogFile;

class LogHnd : public PxJob::Job {
public:
	int fd;
	std::string name;
	std::string cls;
	std::string sbuf;
	~LogHnd() {
		close(fd);
		remove(("/run/logger/"+name).c_str());
	}
	PxResult::Result<void> finish() override {
		return PxResult::Null;
	}
	void wrt() {
		auto input = sbuf;
		sbuf = "";
		std::string terse_input = "console";

		if (input.substr(0, 4) == "\x1B]TS") {
			input = input.substr(4);
			auto vec = PxFunction::split(input, "\x1B]TE", 1);
			if (vec.size() == 2) {
				terse_input = vec[0];
				input = vec[1];
			} else {
				terse_input = "bad-terse-string";
			}
		}

		auto entry = PxLogging::CreateEntry(cls, input, terse_input);
		auto line = entry.serialize()+"\n";

		fwrite(line.c_str(), 1, line.length(), LogFile);
		if (ferror(LogFile)) {
			perror("fwrite (ferror)");
		}
		fflush(LogFile);
		if (ferror(LogFile)) {
			perror("fflush (ferror)");
		}
	}
	PxResult::Result<void> tick() override {
		char buf[PXL_BUF_SIZE];
		int nbytes = read(fd, buf, PXL_BUF_SIZE);
		if (nbytes < 0) {
			if (errno == EAGAIN) return PxResult::Null;
			return PxResult::Result<void>("LogHnd::tick / read", errno);
		} else if (nbytes == 0) {
			if (sbuf.length() > 0) wrt();
			finishing = true;
			return PxResult::Null;
		}
		char *buf2 = buf;
		for (size_t i = 0; i < nbytes; i++) {
			if (buf2[i] == '\n') {
				sbuf += std::string(buf2, i);
				wrt();
				buf2 += i+1;
				nbytes -= i+1;
				i = -1;
			}
		}
		sbuf += std::string(buf2, nbytes);
		return PxResult::Null;
	}
};


PxResult::Result<void> on_command(PxIPC::EventContext<char> *ctx) {
	auto spl = PxFunction::split(ctx->command, " ");

	if (spl.size() < 1) return PxResult::Result<void>("PxLogger / on_command (not enough args)", EINVAL);
	if (spl[0] == "mklog") {
		auto l = std::make_shared<LogHnd>();
		if (spl.size() < 3) return PxResult::Result<void>("PxLogger / on_command (not enough args)", EINVAL);

		std::string name = spl[1];
		if (name.find("/") != std::string::npos) return PxResult::Result<void>("PxLogger / on_command", EBADF);
		
		std::string cls = spl[2];
		l->name = name;
		l->cls = cls;
		
		remove(("/run/logger/"+name).c_str());
		if (mkfifo(("/run/logger/"+name).c_str(), 0600) < 0) {
			return PxResult::Result<void>("PxLogger / on_command / mkfifo", errno);
		}
		if (chown(("/run/logger/"+name).c_str(), 0, 0) < 0) {
			remove(("/run/logger/"+name).c_str());
			return PxResult::Result<void>("PxLogger / on_command / chown", errno);
		}
		int fd = open(("/run/logger/"+name).c_str(), O_RDONLY);
		if (fd < 0) {
			remove(("/run/logger/"+name).c_str());
			return PxResult::Result<void>("PxLogger / on_command / open", errno);
		}
		l->fd = fd;
		PxFunction::setnonblock(fd);

		jobs.AddJob(l);
	}

	return PxResult::Null;
}

int main() {
	if (mkdir("/var/log", 0644) < 0 && errno != EEXIST) {
		perror("mkdir(\"/var/log/parallax\")");
		return 1;
	}
	if (mkdir("/var/log/parallax", 0644) < 0 && errno != EEXIST) {
		perror("mkdir(\"/var/log/parallax\")");
		return 1;
	}
	if (mkdir("/run/logger", 0644) < 0 && errno != EEXIST) {
		perror("mkdir(\"/run/logger\")");
		return 1;
	}
	if (unlink("/var/log/parallax/current") < 0 && errno != ENOENT) {
		perror("unlink(\"/var/log/parallax/current\")");
		return 1;
	}
	std::string basename = PxLogging::FormatNow();
	std::string logname = "/var/log/parallax/"+basename;
	LogFile = fopen(logname.c_str(), "w");
	if (ferror(LogFile)) {
		perror("fopen (ferror)");
	}
	fchmod(fileno(LogFile), 0644);
	if (symlink(basename.c_str(), "/var/log/parallax/current") < 0) {
		perror("symlink(..., \"/var/log/parallax/current\")");
		return 1;
	}

	serv.Init("/run/parallax-logger.sock").assert("PxLogger");
	jobs.AddJob(std::make_shared<PxIPC::ServerJob<char>>(&serv));
	serv.on_command = on_command;

	while (true) {
		usleep(5000);
		jobs.tick();
	}
}
