
#include <PxArg.hpp>
#include <string>

namespace PxArg {
	PxResult::Result<strvec> parseArgs(ArgParser *parser, strvec strargs) {
		bool positionalMode = false;
		strvec positionals;
		size_t curPositional = 0;
		for (size_t n = 1; n < strargs.size(); n++) {
			auto i = strargs[n];
			if (i.length() >= 1 && i.at(0) == '-' && !positionalMode) {
				if (i.length() >= 2 && i.at(1) == '-') {
					if (i == "--") {
						positionalMode = true;
						continue;
					}
					// Long arg parse
					bool foundArg = false;
					for (auto &m : parser->unorderedArgs) {
						if ("--"+m->longName == i) {
							foundArg = true;
							if (!m->acceptsValue()) {
								m->activate("");
								break;
							} else {
								if (++n >= strargs.size()) {
									return PxResult::Result<strvec>("PxArg::parseArgs (bad argument)", EINVAL);
								}
								std::string valuePass = strargs[n];
								m->activate(valuePass);
								break;
							}
						}
					}
					if (!foundArg) return PxResult::Result<strvec>("PxArg::parseArgs (bad argument)", EINVAL);
					continue;
				}
				// Short arg(s) parse
				for (size_t c = 1; c < i.length(); c++) {
					bool foundArg = false;
					bool foundValArg = false;
					for (auto &m : parser->unorderedArgs) {
						if (m->shortName == i.at(c)) {
							foundArg = true;
							if (!m->acceptsValue()) m->activate("");
							else {
								foundValArg = true;
								std::string valuePass = i.substr(c+1);
								if (valuePass.length() == 0) {
									if (++n >= strargs.size()) {
										return PxResult::Result<strvec>("PxArg::parseArgs (bad argument)", EINVAL);
									}
									valuePass = strargs[n];
								}
								m->activate(valuePass);
								break;
							}
						}
					}
					if (!foundArg) return PxResult::Result<strvec>("PxArg::parseArgs (bad argument)", EINVAL);
					if (foundValArg) break;
				}
			} else if (curPositional < parser->positionalArgs.size()) {
				if (!parser->positionalArgs[curPositional]->validate()) {
					return PxResult::Result<strvec>("PxArg::parseArgs (bad argument)", EINVAL);
				}
				parser->positionalArgs[curPositional]->put(i);
				curPositional++;
			} else positionals.push_back(i);
		}
		for (auto i : parser->positionalArgs) {
			if (!i->optional && !i->active) {
				return PxResult::Result<strvec>("PxArg::parseArgs (missing argument)", EINVAL);
			}
		}
		for (auto i : parser->unorderedArgs) {
			if (!i->optional && !i->active) {
				return PxResult::Result<strvec>("PxArg::parseArgs (missing argument)", EINVAL);
			}
		}
		return positionals;
	}

	void printHelp(ArgParser *parser) {
		std::cout << "Usage: " << parser->cmdline;

		for (auto i : parser->positionalArgs) {
			std::cout << " " << i->usage();
		}

		std::cout << "\n" << parser->description << "\n\n";

		std::cout << "Arguments:\n";
		for (auto i : parser->positionalArgs) {
			std::string leftSide = i->name;
			while (leftSide.length() < 20) {
				leftSide += ' ';
			}
			std::cout << "    " << leftSide << i->description << "\n";
		}
		for (auto i : parser->unorderedArgs) {
			std::string leftSide;
			if (i->shortName == 0) {
				leftSide = "--"+i->longName;
			} else {
				leftSide = (std::string)"-"+i->shortName+", --"+i->longName;
			}
			while (leftSide.length() < 20) {
				leftSide += ' ';
			}
			std::cout << "    " << leftSide << i->description << "\n";
		}
		for (auto i : parser->positionalArgs) {
			i->printHelp();
		}
	}
};
