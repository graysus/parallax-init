#include <PxIPC.hpp>

PxIPC::Client cli;

int main() {
	cli.Connect("/tmp/ipctest.sock").assert();
	cli.Write("Hello World!\0").assert();
}
