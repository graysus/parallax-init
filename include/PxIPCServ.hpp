
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <PxResult.hpp>
#include <PxJob.hpp>

#ifndef PXIS
#define PXIS

#define PXIS_BUF_SIZE 4096
#define PXIS_TIMEOUT 5

#define PXIS_CON 0

#define PXIS_ERR_ONCON 0
#define PXIS_ERR_ONDISCON 4
#define PXIS_ERR_ONCMD 8
// Errors within errors are ignored.

#define PXIS_DISCON_CLOSED 0
#define PXIS_DISCON_TIMEOUT 1
#define PXIS_DISCON_SERVER_CLOSE 2

#define PXIS_CMD_NORMAL 0
#define PXIS_CMD_CUTOFF 1

#define PXIS_MASK_CALL 12
#define PXIS_MASK_ARG 3

namespace PxIPC {
	struct Raw_Connection;
	struct Raw_Server;
	template<typename T> class Server;
	template<typename T> struct EventContext;

	typedef void (*conncb)(Raw_Server*, Raw_Connection*, int, PxResult::Result<void>*);
	template<typename T> using IPCCallback = PxResult::Result<void>(*)(EventContext<T>*);
	
	struct Raw_Connection {
		int fd;
		char cur_command[PXIS_BUF_SIZE];
		char cur_response[PXIS_BUF_SIZE];
		size_t cursor = 0;
		size_t cursor_send = 0;
		size_t cursor_send_len = 0;
		int to_purge = -1;
		void *userdata;
		time_t lastmsg;
		size_t handle = 0;
	};
	struct Raw_Server {
		std::string servroot;
		int fd;
		struct sockaddr_un *addr;
		std::vector<Raw_Connection*> connections;
		void *userdata;
		conncb on_connect = NULL;
		conncb on_command = NULL;
		conncb on_disconnect = NULL;
		conncb on_error = NULL;
		size_t curhandle = 0;
	};
	typedef Raw_Connection Connection;

	PxResult::Result<void> Raw_ServerUpdate(Raw_Server* serv);
	PxResult::Result<Raw_Server*> Raw_ServerInit(std::string servroot = "/run/parallax.sock");
	PxResult::Result<void> Raw_ServerDelete(Raw_Server* serv);
	PxResult::Result<void> Raw_ServConnSend(Raw_Server* serv, Raw_Connection* conn, const std::string& data);
	void Raw_DefError(Raw_Server*, Raw_Connection*, int, PxResult::Result<void>*);
	PxResult::Result<Raw_Connection*> Raw_ServerGetConnection(Raw_Server *serv, size_t hndl);

	template<typename T> struct EventContext {
		Server<T>* serv;
		Connection* conn;
		T* conndata;
		int flags;
		PxResult::Result<void> err;  // Only for event on_error
		std::string command;  // Only for event on_command

		PxResult::Result<void> reply(std::string data) {
			if (!PxFunction::endsWith(data, (std::string)""+'\0')) {
				data += '\0';
			}
			return Raw_ServConnSend(serv->serv->serv, conn, data);
		}
	};

	// Wrapper
	struct Raw_Server2 {
		Raw_Server *serv;
		~Raw_Server2() {
			Raw_ServerDelete(serv);
			// Ignore errors.
		}
	};

	template<typename T> void ServerConnect(Raw_Server* serv, Raw_Connection* conn, int flags, PxResult::Result<void> *result) {
		// Assume serv->userdata is wrapped server
		auto wserv = (Server<T>*)serv->userdata;
		conn->userdata = NULL;
		EventContext<T> ctx = {wserv, conn, (T*)conn->userdata, flags};
		if (wserv->on_connect != NULL) *result = wserv->on_connect(&ctx);
	}	
	template<typename T> void ServerDisconnect(Raw_Server* serv, Raw_Connection* conn, int flags, PxResult::Result<void> *result) {
		// Assume serv->userdata is wrapped server
		auto wserv = (Server<T>*)serv->userdata;
		EventContext<T> ctx = {wserv, conn, (T*)conn->userdata, flags};
		if (wserv->on_disconnect != NULL) *result = wserv->on_disconnect(&ctx);
		// Additionally, delete the connection data if it exists.
		if (ctx.conndata != NULL) delete ctx.conndata;
		conn->userdata = NULL;
	}
	template<typename T> void ServerCommand(Raw_Server* serv, Raw_Connection* conn, int flags, PxResult::Result<void> *result) {
		// Assume serv->userdata is wrapped server
		auto wserv = (Server<T>*)serv->userdata;
		EventContext<T> ctx = {wserv, conn, (T*)conn->userdata, flags};
		ctx.command = std::string(conn->cur_command, conn->cursor-1);
		if (wserv->on_command != NULL) *result = wserv->on_command(&ctx);
	}
	template<typename T> void ServerError(Raw_Server* serv, Raw_Connection* conn, int flags, PxResult::Result<void> *result) {
		// Assume serv->userdata is wrapped server
		auto wserv = (Server<T>*)serv->userdata;
		EventContext<T> ctx = {wserv, conn, (T*)conn->userdata, flags};
		ctx.err = *result;
		if (wserv->on_error != NULL) *result = wserv->on_error(&ctx);
		else Raw_DefError(serv, conn, flags, result);
	}

	template<typename T> class Server {
	private:
		std::shared_ptr<Raw_Server2> serv;
	public:
		friend EventContext<T>;
		IPCCallback<T> on_connect = NULL;
		IPCCallback<T> on_disconnect = NULL;
		IPCCallback<T> on_error = NULL;
		IPCCallback<T> on_command = NULL;

		Server() {}
		PxResult::Result<void> Init(std::string servroot) {
			auto res = Raw_ServerInit(servroot);
			if (!res.eno) {
				serv.reset(new Raw_Server2({res.assert()}));
				serv->serv->userdata = (void*)this;

				serv->serv->on_connect = ServerConnect<T>;
				serv->serv->on_disconnect = ServerDisconnect<T>;
				serv->serv->on_command = ServerCommand<T>;
				serv->serv->on_error = ServerError<T>;
			}
			on_connect = NULL;
			return PxResult::Clear(res).merge("PxIPC::Server::Init");
		}
		PxResult::Result<void> Update() {
			return Raw_ServerUpdate(serv->serv);
		}
		PxResult::Result<Connection*> getConnection(size_t handle) {
			return Raw_ServerGetConnection(serv->serv, handle);
		}
		PxResult::Result<void> Send(Connection *conn, std::string data) {
			if (!PxFunction::endsWith(data, (std::string)""+'\0')) {
				data += '\0';
			}
			return Raw_ServConnSend(serv->serv, conn, data);
		}
	};

	template<typename T> class ServerJob : public PxJob::Job {
	public:
		Server<T> *sv;
		ServerJob(Server<T> *Serv) {
			sv = Serv;
		}
		PxResult::Result<void> tick() {
			return sv->Update();
		}
	};
}

#endif
