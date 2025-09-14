
#include <PxLog.hpp>
#include <PxResult.hpp>
#include <PxFunction.hpp>
#include <cerrno>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>

#ifndef PXIPC
#define PXIPC

namespace PxIPC {
	struct Raw_Client {
		std::string cliroot;
		int fd;
		struct sockaddr_un *addr;
	};
	
	PxResult::Result<Raw_Client*> Raw_ClientConnect(std::string cliroot);
	PxResult::Result<void> Raw_ClientWrite(Raw_Client* client, std::string content);
	PxResult::Result<void> Raw_ClientRead(Raw_Client* client, std::string &content);
	void Raw_ClientFree(Raw_Client *client);
	
	struct Raw_Client2 {
		Raw_Client *cli;
		~Raw_Client2() {
			if (cli != NULL) {
				Raw_ClientFree(cli);
			}
		}
	};

	class Client {
		std::shared_ptr<Raw_Client2> cli;
	public:
		Client() {}
		PxResult::Result<void> Connect(std::string cliroot) {
			auto res = Raw_ClientConnect(cliroot);
			if (res.eno) return PxResult::Clear(res);
			cli.reset(new Raw_Client2({res.assert()}));
			return PxResult::Null;
		}
		PxResult::Result<void> Write(std::string data) {
			if (!PxFunction::endsWith(data, (std::string)""+'\0')) {
				data += '\0';
			}
			if (cli.get() == NULL || cli->cli == NULL) {
				return PxResult::Result<void>("PxIPC::Client::Write (bad pointer)", ENODEV);
			}
			return Raw_ClientWrite(cli->cli, data);
		}
		PxResult::Result<std::string> Read() {
			if (cli.get() == NULL || cli->cli == NULL) {
				return PxResult::Result<std::string>("PxIPC::Client::Write (bad pointer)", ENODEV);
			}
			std::string readvalue;
			auto res = Raw_ClientRead(cli->cli, readvalue);
			return PxResult::Attach(res, readvalue);
		}
	};
}

#endif
