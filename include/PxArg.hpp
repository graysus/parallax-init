
// This code worked first try, somehow
#include <string>
#include <vector>
#include <PxFunction.hpp>
#include <PxResult.hpp>

#ifndef PXARG
#define PXARG

namespace PxArg {
	typedef std::vector<std::string> strvec;
	struct PositionalArgument {
		bool active = false;
		bool optional = false;
		std::string name;
		std::string description;
		std::string value = "";

		PositionalArgument(std::string name, std::string description, bool optional = false)
			: name(name), description(description), optional(optional) {}
		
		virtual void printHelp() {

		}
		virtual bool validate() {
			return true;
		}
		virtual std::string usage() {
			return optional ? "["+name+"]" : name;
		}
		virtual void put(std::string raw_value) {
			value = raw_value;
			active = true;
		}
	};

	struct SelectOption {
		std::string name;
		std::string description;
	};

	struct SelectArgument : public PositionalArgument {
		std::vector<SelectOption> allowed_values;

		SelectArgument(std::string name, std::string description, bool optional = false)
			: PositionalArgument(name, description, optional) {}
		
		void addOption(std::string name, std::string description) {
			allowed_values.push_back({name, description});
		}

		virtual void printHelp() {
			std::cout << "\n";
			std::cout << name << " may be one of:\n";
			for (auto &i : allowed_values) {
				std::string leftSide = i.name;
				while (leftSide.length() < 20) {
					leftSide += ' ';
				}
				std::cout << "    " << leftSide << i.description << "\n";
			}
		}
		virtual bool validate() {
			return true;
		}
		virtual std::string usage() {
			return optional ? "["+name+"]" : name;
		}
		virtual void put(std::string raw_value) {
			value = raw_value;
			active = true;
		}
	};

	struct Argument {
		bool active;
		char shortName;
		bool optional;
		std::string longName;
		std::string description;
		Argument(std::string LongName, char ShortName = '\0', std::string description = "", bool optional = true)
			: longName(LongName), shortName(ShortName), description(description), optional(optional) {
			active = false;
		}
		virtual bool acceptsValue() {
			return false;
		}
		virtual void activate(std::string value) {
			active = true;
		}
	};

	struct ValueArgument : public Argument {
		std::string value;
		ValueArgument(std::string LongName, char ShortName = '\0', std::string description = "", bool optional = true)
			: Argument(LongName, ShortName, description, optional) {}
		bool acceptsValue() override {
			return true;
		}
		void activate(std::string value) override {
			active = true;
			this->value = value;
		}
	};

	struct ArgParser;

	PxResult::Result<strvec> parseArgs(ArgParser *parser, strvec strargs);
	void printHelp(ArgParser *parser);

	struct ArgParser {
		std::vector<PositionalArgument*> positionalArgs;
		std::vector<Argument*> unorderedArgs;
		std::string cmdline;
		std::string description;
		
		ArgParser(
			const std::vector<PositionalArgument*> &positionalArgs,
			const std::vector<Argument*> &unorderedArgs
		) : positionalArgs(positionalArgs), unorderedArgs(unorderedArgs) {}

		PxResult::Result<strvec> parseArgs(strvec strargs) {
			return PxArg::parseArgs(this, strargs);
		}
		void printHelp() {
			return PxArg::printHelp(this);
		};
	};
}
#endif
