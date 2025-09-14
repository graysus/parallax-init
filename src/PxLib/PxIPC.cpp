
#include <PxIPC.hpp>

#include <PxLog.hpp>
#include <PxResult.hpp>
#include <PxFunction.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>

namespace PxIPC {
	PxResult::Result<Raw_Client*> Raw_ClientConnect(std::string cliroot) {
		// Most of this code is the same as the code for the server.
		Raw_Client *client = new Raw_Client();
		client->fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (client->fd < 0) {
			delete client;
			return PxResult::Result<Raw_Client*>("PxIPC::Raw_ClientConnect / socket", errno);
		}

		// Create the address
		auto addr = new struct sockaddr_un();
		client->addr = addr;
		memset(addr, 0, sizeof(sockaddr_un));
		addr->sun_family = AF_UNIX;
		strncpy(addr->sun_path, cliroot.c_str(), std::min(cliroot.length(), sizeof(addr->sun_path)-1));

		// Connect to the server
		if (connect(client->fd, (const sockaddr*)addr, sizeof(*addr)) != 0) {
			close(client->fd);
			delete client;
			delete addr;
			return PxResult::Result<Raw_Client*>("PxIPC::Raw_ClientConnect / socket", errno);
		}
		return client;
	}
	PxResult::Result<void> Raw_ClientWrite(Raw_Client* client, std::string content) {
		size_t wrn = 0;
		while (wrn < content.length()) {
			auto ret = send(client->fd, content.c_str()+wrn, content.length(), MSG_NOSIGNAL);
			if (ret < 0) {
				return PxResult::Result<void>("Raw_ClientWrite / write", errno);
			} else if (ret == 0) {
				return PxResult::Result<void>("Raw_ClientWrite / write (connection terminated)", EPIPE);
			}
			wrn += ret;
		}
		return PxResult::Null;
	}
	PxResult::Result<void> Raw_ClientRead(Raw_Client* client, std::string &content) {
		char buf[BUFSIZ];
		content.clear();
		PxFunction::setnonblock(client->fd, true);
		while (true) {
			auto ret = recv(client->fd, buf, BUFSIZ, MSG_NOSIGNAL);
			if (ret < 0) {
				if (errno == EAGAIN) {
					usleep(100);
					continue;
				}
				return PxResult::Result<void>("Raw_ClientWrite / write", errno);
			} else if (ret == 0) {
				return PxResult::Result<void>("Raw_ClientWrite / write (connection terminated)", EPIPE);
			}
			content.reserve(ret);
			for (size_t i = 0; i < ret; i++) {
				if (buf[i] == 0) {
					// complete successfully
					return PxResult::Null;
				} else {
					content += buf[i];
				}
			}
		}
		PxFunction::setnonblock(client->fd, false);
	}
	void Raw_ClientFree(Raw_Client *client) {
		close(client->fd);
		delete client->addr;
		delete client;
	}
}
