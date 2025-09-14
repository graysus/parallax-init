
#include <PxIPCServ.hpp>
#include <unistd.h>
#include <iostream>

void on_connect(PxIPC::Raw_Server *serv, PxIPC::Raw_Connection *conn, int flags, PxResult::Result<void> *result) {
	std::cout << "New connection\n";
}
void on_command(PxIPC::Raw_Server *serv, PxIPC::Raw_Connection *conn, int flags, PxResult::Result<void> *result) {
	std::cout << "Command: " << conn->cur_command << "\n";
}
void on_disconnect(PxIPC::Raw_Server *serv, PxIPC::Raw_Connection *conn, int flags, PxResult::Result<void> *result) {
	std::cout << "Connection ended\n";
}

int main() {
	PxIPC::Raw_Server *serv = PxIPC::Raw_ServerInit("/tmp/ipctest.sock").assert("main");
	serv->on_connect = on_connect;
	serv->on_command = on_command;
	serv->on_disconnect = on_disconnect;
	std::cout << "Server created.\n";
	while (true) {
		usleep(100000);
		PxIPC::Raw_ServerUpdate(serv).assert("main");
	}
}
