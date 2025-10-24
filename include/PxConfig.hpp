#include <PxFunction.hpp>
#include <PxResult.hpp>
#include <PxLog.hpp>
#include <PxState.hpp>
#include <map>
#include <vector>

#ifndef PXCONF
#define PXCONF
namespace PxConfig {
	enum ConfigParseMode {
		Normal, Multiline
	};

	std::vector<std::string> Escape(std::string baseString, std::string argument, int msplit, std::string sep, bool multi = false);

	class conf;
	PxResult::Result<void> ConfSetValue(conf *c, std::string key, std::string value);
	PxResult::Result<conf> ReadConfig(std::string filename, std::string argument = "");
	class conf  {
	private:
		std::map<std::string, std::vector<std::string>> vec_properties ;
	public:
		friend PxResult::Result<conf> ReadConfig(std::string filename, std::string argument);

		std::string def_arg;
		std::string separator = ":";
		int maxargs = 9;

		void RawAdd(std::string key, std::string value) {
			vec_properties[key].push_back(value);
		}

		void ReadValue(const std::string &key, std::vector<std::string> &output, const std::string &argument = "") {
			auto argp = argument.empty() ? &def_arg : &argument;

			for (auto &def : vec_properties[key]) {
				auto escaped = Escape(def, *argp, maxargs-1, separator);
				for (auto &i : escaped) {
					output.push_back(i);
				}
			}
		}

		std::string QuickRead(const std::string &key, const std::string &argument = "") {
			std::vector<std::string> output;
			ReadValue(key, output, argument);
			if (output.size() == 0) return "";
			return output[0];
		}

		std::vector<std::string> QuickReadVec(const std::string &key, const std::string &argument = "") {
			std::vector<std::string> output;
			ReadValue(key, output, argument);
			return output;
		}
	};
};
#endif
