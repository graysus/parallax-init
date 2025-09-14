#include <PxFunction.hpp>
#include <memory>
#include <PxResult.hpp>
#include <cstdio>
#include <iterator>
#include <string>
#include <unistd.h>
#include <vector>

#ifndef PXLOG
#define PXLOG
namespace PxLog {
	extern int curID;
	enum LogStatus {
		Pending, Success, Partial, Fail
	};
	struct LogTask {
		std::string me;
		std::string terse;
		LogStatus status = Pending;
		int id;
		virtual std::string repr() {
			std::string prepend = "";
			switch (status) {
				case Pending:
					prepend = "";
					break;
				case Success:
					prepend = "Finished: ";
					break;
				case Partial:
					prepend = "Partial: ";
					break;
				case Fail:
					prepend = "Fail: ";
					break;
			}
			return prepend+me;
		}
	};
	class Log {
	public:
		std::vector<std::shared_ptr<PxLog::LogTask>> tasks;
		bool oscillation = false;
		int linkfd = -1;
		int suppressors = 0;
		Log(int fd = -1) {
			linkfd = fd;
		}
		void suppress(bool clear = true) {
			if (!suppressed()) {
				if (clear) {
					top();
					std::cout << "\r\x1b[0J\r";
					std::cout.flush();
				}
			}
			suppressors++;
		}
		void unsuppress() {
			suppressors--;
			if (!suppressed()) {
				std::cout << "\r\x1b[0J\r";
				printTasks();	
			}
		}
		bool suppressed() {
			return suppressors > 0;
		}
		int osc() {
			return oscillation ? 90 : 97;
		}
		void printTask(LogTask *t) {
			if (suppressed()) return;
			std::cout << "\r\x1b[2K\x1b[90m[\x1b[" << osc() << "m*\x1b[90m] \x1b[97m"
				<< t->repr() << "\n";
		}
		void printTasks() {
			if (suppressed()) return;
			for (auto &i : tasks) {
				printTask(i.get());
			}
			std::cout << "\x1b[0m";
			std::cout.flush();
		}
		void top() {
			if (suppressed()) return;
			if (tasks.size() > 0) std::cout << "\x1b[" << tasks.size() << "A\r";
		}
		void updateTasks() {
			if (suppressed()) return;
			if (tasks.size() == 0) return;
			top();
			std::cout << "\r\x1b[C";
			for (auto &_ : tasks) {
				std::cout << "\x1b[" << osc() << "m*\x1b[B\x1b[D";
			}
			std::cout << "\r\x1b[0m";
			std::cout.flush();
		}
		void println(std::string str) {
			if (suppressed()) return;
			top();
			auto lines = PxFunction::split(str, "\n");
			for (auto &i : lines) {
				std::cout << "\x1b[2K" << i << "\n";
			}
			printTasks();
		}
		void _logwrite(std::string str) {
			size_t count = 0;
			while (count < str.length()) {
				auto write_res = write(linkfd, str.c_str()+count, str.length()-count);
				if (write_res < 0) {
					perror("Failed writing log file");
				}
				count += write_res;
			}
		}
		void printlnlog(std::string str, std::string terse) {
			if (linkfd == -1) return;
			if (terse != "") {
				std::string tersewrite = "\x1B]TS" + terse + "\x1B]TE";
				_logwrite(tersewrite);
			}
			_logwrite(str);
			_logwrite("\n");
		}
		void header(std::string str, std::string terse = "head") {
			println("\x1b[97;1m=== " + str + "\x1b[0m");
			printlnlog("[HEAD] "+str, terse);
		}
		void taskSuccess(std::string str, std::string terse = "success") {
			println("\x1b[90m[\x1b[92m*\x1b[90m] \x1b[97m"+str);
			printlnlog("[SUCC] "+str, terse);
		}
		void taskFail(std::string str, std::string terse = "failure") {
			println("\x1b[90m[\x1b[91m*\x1b[90m] \x1b[97m"+str);
			printlnlog("[FAIL] "+str, terse);
		}
		void taskPartial(std::string str, std::string terse = "partial") {
			println("\x1b[90m[\x1b[93m*\x1b[90m] \x1b[97m"+str);
			printlnlog("[PART] "+str, terse);
		}
		void info(std::string str, std::string terse = "info") {
			println("  \x1b[94m(i) \x1b[37m"+str);
			printlnlog("[INFO]   (i) "+str, terse);
		}
		void warn(std::string str, std::string terse = "warn") {
			println("  \x1b[93m!!! \x1b[37m"+str);
			printlnlog("[WARN]   !!! "+str, terse);
		}
		void error(std::string str, std::string terse = "error") {
			println("  \x1b[91m!!! \x1b[37m"+str);
			printlnlog("[ERRO]   !!! "+str, terse);
		}
		int newTask(LogTask* task) {
			std::shared_ptr<LogTask> x;
			x.reset(task);
			tasks.push_back(x);
			task->id = curID++;
			printTask(task);
			printlnlog("[TASK] "+task->repr(), "newtask "+task->terse);
			return x->id;
		}
		void completeTask(int id, LogStatus status) {
			for (size_t i = 0; i < tasks.size(); i++) {
				auto x = tasks[i];
				if (x->id == id) {
					x->status = status;
					switch (status) {
						case Success:
							tasks.erase(std::next(tasks.begin(), i), std::next(tasks.begin(), i+1));
							if (!suppressed()) std::cout << "\x1b[A";
							taskSuccess(x->repr(), "success "+x->terse);
							return;
						case Partial:
							tasks.erase(std::next(tasks.begin(), i), std::next(tasks.begin(), i+1));
							if (!suppressed()) std::cout << "\x1b[A";
							taskPartial(x->repr(), "partial " + x->terse);
							return;
						case Fail:
							tasks.erase(std::next(tasks.begin(), i), std::next(tasks.begin(), i+1));
							if (!suppressed()) std::cout << "\x1b[A";
							taskFail(x->repr(), "fail "+x->terse);
							return;
						case Pending:
							// Nothing changed, do not update
							return;
					}
				}
			}
		}
	};
	extern Log log;
}
#endif
