
#include <PxResult.hpp>
#include <PxFunction.hpp>
#include <algorithm>
#include <bits/types/struct_timeval.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <iterator>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <vector>
#include <time.h>

#define PXIS_BUF_SIZE 4096
#define PXIS_TIMEOUT 5
#define PXIS_SEL_TIMEOUT 2

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

	typedef void (*conncb)(Raw_Server*, Raw_Connection*, int, PxResult::Result<void>*);

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
	PxResult::Result<void> Raw_ServConnSend(Raw_Server* serv, Raw_Connection* conn, const std::string &data) {
		if (conn->cursor_send_len != 0) {
			return PxResult::Result<void>("Raw_ServConnSend (try again)", EAGAIN);
		}
		memcpy(conn->cur_response, data.c_str(), data.length());
		conn->cursor_send = 0;
		conn->cursor_send_len = data.length();
		return PxResult::Null;
	}
	PxResult::Result<void> Raw_ServConnUpdate1(Raw_Server* serv, Raw_Connection* conn) {
		char buf[PXIS_BUF_SIZE];
		int numread;

		numread = recv(conn->fd, buf, PXIS_BUF_SIZE, 0);
		if (numread < 0 && errno != EAGAIN) {
			return PxResult::Result<void>("PxIPC::Raw_ServConnUpdate / read", errno);
		} else if (numread < 0 && errno == EAGAIN) {
			// if (time(NULL) - conn->lastmsg > PXIS_TIMEOUT) {
			// 	conn->to_purge = PXIS_DISCON_TIMEOUT;
			// }
			// Nothing to see here, try again later.
			// TODO: please time out shit please please pslepwsleap
			return PxResult::Null;
		}
		if (numread > 0) {
			conn->lastmsg = time(NULL);
		}
		for (int i = 0; i < numread; i++) {
			if (conn->cursor >= PXIS_BUF_SIZE) {
				return PxResult::Result<void>("PxIPC::Raw_ServConnUpdate (while copying buffer)", ENOSPC);
			}
			conn->cur_command[conn->cursor] = buf[i];
			conn->cursor++;
			if (buf[i] == 0) {
				if (serv->on_command != NULL) {
					PxResult::Result<void> res;
					serv->on_command(serv, conn, PXIS_CMD_NORMAL, &res);
					if (res.eno) serv->on_error(serv, conn, PXIS_ERR_ONCMD | PXIS_CMD_NORMAL, &res);
				}
				conn->cursor = 0;
			}
		}
		if (numread == 0) {
			conn->to_purge = PXIS_DISCON_CLOSED;
		}
		return PxResult::Null;
	}

	PxResult::Result<void> Raw_ServConnUpdate2(Raw_Server* serv, Raw_Connection* conn) {
		if (conn->cursor_send_len == 0) {
			return PxResult::Null;
		}
		auto numread = send(conn->fd, conn->cur_response + conn->cursor_send, conn->cursor_send_len - conn->cursor_send, 0);
		if (numread < 0) {
			if (errno == EAGAIN) return PxResult::Null;
			return PxResult::Result<void>("Raw_ServConnUpdate2 / send", errno);
		}
		conn->cursor_send += numread;
		if (conn->cursor_send == conn->cursor_send_len) {
			conn->cursor_send = 0;
			conn->cursor_send_len = 0;
		}
		return PxResult::Null;
	}

	void Raw_DefError(Raw_Server *serv, Raw_Connection *conn, int flags, PxResult::Result<void> *err) {
		if (err == NULL) {
			std::cerr << "NULL passed as status to on_error, ignoring.\n";
			return;
		} else if (err->eno == 0) {
			std::cerr << "Successful result passed as status to on_error, ignoring.\n";
			return;
		}

		std::string function = "";

		switch (flags & PXIS_MASK_CALL) {
			case PXIS_ERR_ONCON:
				function = "on_connect";
				break;
			case PXIS_ERR_ONDISCON:
				function = "on_disconnect";
				break;
			case PXIS_ERR_ONCMD:
				function = "on_cmd";
				break;
			default:
				function = "unknown";
				break;
		}
		
		errno = err->eno;
		perror(("Ignoring exception in "+function+": "+err->funcName).c_str());
	}
	void Raw_HandleDisconnects(Raw_Server *serv, bool processCommands = true) {
		for (int i = 0; i < serv->connections.size(); i++) {
			auto conn = serv->connections[i];
			if (conn->to_purge >= 0) {
				if (conn->cursor != 0 && processCommands) {
					// Incomplete command, emulate a null byte and run anyway.
					if (conn->cursor > PXIS_BUF_SIZE) {
						// ignore. This is too much of an edge case
					} else {
						conn->cur_command[conn->cursor] = 0;
						conn->cursor++;
						if (serv->on_command != NULL) {
							PxResult::Result<void> res;
							serv->on_command(serv, conn, PXIS_CMD_CUTOFF, &res);
							if (res.eno) serv->on_error(serv, conn, PXIS_ERR_ONCON | PXIS_CON, &res);
						}
					}
				}
				// Close connection
				if (serv->on_disconnect != NULL) {
					PxResult::Result<void> res;
					serv->on_disconnect(serv, conn, conn->to_purge, &res);
					if (res.eno) serv->on_error(serv, conn, PXIS_ERR_ONDISCON | conn->to_purge, &res);
				}
				close(conn->fd);
				delete conn;
				// As for user_data, the user is responsible for freeing the data.
				// The user should use on_disconnect for this.

				// C++ moment
				serv->connections.erase(
						std::next(serv->connections.begin(), i),
						std::next(serv->connections.begin(), i+1)
				);
				i--;
			}
		}
	}
	PxResult::Result<void> Raw_ServerUpdate(Raw_Server *serv) {
		socklen_t addrlen = sizeof(*serv->addr);
		int result = accept(serv->fd, (sockaddr*)serv->addr, &addrlen);
		if (result < 0 && errno != EAGAIN) {
			return PxResult::Result<void>("PxIPC::Raw_ServerUpdate", errno);
		} else if (result >= 0) {
			auto conn = new Raw_Connection();
			conn->fd = result;
			PxFunction::setnonblock(conn->fd);
			conn->userdata = NULL;
			conn->lastmsg = time(NULL);
			serv->connections.push_back(conn);
			conn->handle = ++serv->curhandle;
			if (serv->on_connect != NULL) {
				PxResult::Result<void> res;
				serv->on_connect(serv, conn, PXIS_CON, &res);
				if (res.eno) serv->on_error(serv, conn, PXIS_ERR_ONCON | PXIS_CON, &res);
			}
		}
		for (auto& i : serv->connections) {
			auto status = Raw_ServConnUpdate1(serv, i);
			if (status.eno) return status.merge("PxIPC::Raw_ServerUpdate");
			status = Raw_ServConnUpdate2(serv, i);
			if (status.eno) return status.merge("PxIPC::Raw_ServerUpdate");
		}
		Raw_HandleDisconnects(serv);
		return PxResult::Null;
	}
	PxResult::Result<Raw_Server*> Raw_ServerInit(std::string servroot = "/run/parallax.sock") {
		remove(servroot.c_str());
		Raw_Server *server = new Raw_Server();
		server->servroot = servroot;
		server->fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (server->fd < 0) {
			delete server;
			return PxResult::Result<Raw_Server*>("PxIPC::Raw_ServerInit / socket", errno);
		}
		// Initialize the address
		// I'm surprised it lets me do that
		auto addr = new struct sockaddr_un();
		server->addr = addr;
		memset(addr, 0, sizeof(*addr));
		addr->sun_family = AF_UNIX;
		strncpy(addr->sun_path, servroot.c_str(), std::min(servroot.length(), sizeof(addr->sun_path)-1));

		// Bind the socket
		if (bind(server->fd, (const sockaddr*)addr, sizeof(*addr)) != 0) {
			close(server->fd);
			delete server;
			delete addr;
			return PxResult::Result<Raw_Server*>("PxIPC::Raw_ServerInit / bind", errno);
		}

		// Listen for connections
		if (listen(server->fd, SOMAXCONN) != 0) {
			close(server->fd);
			delete server;
			delete addr;
			return PxResult::Result<Raw_Server*>("PxIPC::Raw_ServerInit / listen", errno);
		}

		// Set non-blocking
		PxFunction::setnonblock(server->fd);

		// Errors must be handled.
		server->on_error = Raw_DefError;

		return server;
	}
	PxResult::Result<void> Raw_ServerDelete(Raw_Server *serv) {
		Raw_HandleDisconnects(serv);
		for (auto& i : serv->connections) {
			i->to_purge = PXIS_DISCON_SERVER_CLOSE;
		}
		Raw_HandleDisconnects(serv, false);
		close(serv->fd);
		if (remove(serv->servroot.c_str()) != 0) {
			return PxResult::Result<void>("PxIPC::Raw_ServerDelete / remove", errno);
		}
		delete serv->addr;
		delete serv;
		return PxResult::Null;
	}
	PxResult::Result<Raw_Connection*> Raw_ServerGetConnection(Raw_Server *serv, size_t hndl) {
		for (auto i : serv->connections) {
			if (i->handle == hndl) {
				return PxResult::Result(i);
			}
		}
		return PxResult::Result<Raw_Connection*>("Raw_ServerGetConnection", EPIPE);
	}
}
