
#include <PxIPCServ.hpp>
#include <unistd.h>
#include <iostream>
#include <PxResult.hpp>

PxResult::Result<void> on_connect(PxIPC::EventContext<char> *ctx) {
	std::cout << "New connection\n";
	return PxResult::Null;
}
PxResult::Result<void> on_command(PxIPC::EventContext<char> *ctx) {
	std::cout << "Command: " << ctx->command << "\n";
	return PxResult::Null;
}
PxResult::Result<void> on_disconnect(PxIPC::EventContext<char> *ctx) {
	std::cout << "Connection ended\n";
	return PxResult::Null;
}

int main() {
	PxIPC::Server<char> serv;
	serv.Init("/tmp/ipctest.sock").assert("main");
	serv.on_connect = on_connect;
	serv.on_command = on_command;
	serv.on_disconnect = on_disconnect;
	std::cout << "Server created.\n";
	while (true) {
		usleep(100000);
		serv.Update().assert("main");
	}
}
