#include <PxEnv.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <unistd.h>

namespace PxEnv {
    void dump(std::string filename) {
        std::ofstream file(filename, std::ios::binary | std::ios::out);
        for (char** e = environ; *e != NULL; e++) {
            uint32_t sz = strlen(*e);
            file.write((char*)&sz, sizeof(sz));
            file.write(*e, sz*sizeof(char));
        }
    }
    void dumpStrings(std::string filename, std::vector<std::string>& strings) {
        std::ofstream file(filename, std::ios::binary | std::ios::out);
        for (auto &i : strings) {
            uint32_t sz = i.length();
            file.write((char*)&sz, sizeof(sz));
            file.write(i.c_str(), sz*sizeof(char));
        }
    }
    void load(std::string filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::in);
        while (!file.eof()) {
            // My condolences to all that require 4GiB environment variables
            uint32_t sz;
            file.read((char*)&sz, sizeof(sz));
            if (file.fail()) break;

            char* buf = (char*)malloc((sz+1)*sizeof(char));
            file.read(buf, sz*sizeof(char));
            if (file.fail()) break;
            buf[sz] = 0;
            if (putenv(buf) < 0) {
                perror("putenv");
            }
            free(buf);
        }
    }
    void loadStrings(std::string filename, std::vector<std::string>& strings) {
        std::ifstream file(filename, std::ios::binary | std::ios::in);
        while (!file.eof()) {
            // My condolences to all that require 4GiB environment variables
            uint32_t sz;
            file.read((char*)&sz, sizeof(sz));
            if (file.fail()) break;

            char* buf = (char*)malloc((sz+1)*sizeof(char));
            file.read(buf, sz*sizeof(char));
            if (file.fail()) break;
            buf[sz] = 0;
            strings.push_back(std::string(buf, sz));
            free(buf);
        }
    }
}