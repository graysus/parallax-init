#include <filesystem>
#include <iostream>
#include <PxMount.hpp>
#include <libmount/libmount.h>
#include <set>
#include <unistd.h>
#include <PxFunction.hpp>
#include <PxIPC.hpp>

struct checked_t {
    bool checked;
    std::string device;
};

std::string check_online(std::string which) {
    char *spec;

    if (NULL == (spec = mnt_resolve_spec(which.c_str(), NULL))) {
        return "";
    }

    // spec is NOT null
    if (!std::filesystem::exists(spec)) {
        free(spec);
        return "";
    }

    std::string spec_str(spec);
    free(spec);
    return spec_str;
}

bool chkpass(const std::vector<PxMount::FsEntry> &entries, int pass, const std::set<std::string> &ignore) {
    bool failed = false;
    std::vector<std::string> entries_to_check;
    for (auto i : entries) {
        // Skip if pseudo-filesystem.
        if (
            !PxFunction::startsWith(i.source, "/") &&
            !PxFunction::startsWith(i.source, "UUID=") &&
            !PxFunction::startsWith(i.source, "PARTUUID=") &&
            !PxFunction::startsWith(i.source, "LABEL=") &&
            !PxFunction::startsWith(i.source, "ID=") &&
            !PxFunction::startsWith(i.source, "DISKSEQ=") &&
            !PxFunction::startsWith(i.source, "PATH=")
        ) {
            continue;
        }

        if (i.fsck_pass != pass) continue;
        if (ignore.count(i.source)) continue;

        entries_to_check.push_back(i.source);
    }

    bool any_is_unchecked = true;
    std::set<std::string> checked_set = ignore;
    while (any_is_unchecked) {
        any_is_unchecked = false;
        for (auto &device_name : entries_to_check) {
            if (checked_set.count(device_name) != 0) continue;
            std::string online_path = check_online(device_name);


            if (online_path.empty()) {
                any_is_unchecked = true;
                continue;
            } 

            if (checked_set.count(online_path) != 0) {
                checked_set.insert(device_name);
                continue;
            }

            std::cout << "\x1B]TSbegin-check " << device_name << "\x1B]TEchecking " << device_name << "...\n";

            auto status = system(("fsck -p </dev/null "+device_name).c_str());

            if (status & 0b10111100) {
                std::cout << "\x1B]TSend-check fail " << device_name << "\x1B]TEfsck failed \n";
                failed = true;
            } else if (status & 0b00000010) {
                std::cout << "\x1B]TSend-check reboot " << device_name << "\x1B]TESystem must be rebooted.\n";
                PxIPC::Client client;
                client.Connect("/run/parallax-svman.sock");
                client.Write("target reboot\0");
                failed = true;
            } else if (status & 0b00000001) {
                std::cout << "\x1B]TSend-check corrected " << device_name << "\x1B]TEErrors were corrected.\n";
            } else {
                std::cout << "\x1B]TSend-check pass " << device_name << "\x1B]TECheck passed for " << device_name << "\n";
            }

            checked_set.insert(device_name);
        }
        if (any_is_unchecked)
            usleep(1000);
    }
    return failed;
}

int main(int argc, const char* argv[]) {
    struct libmnt_table *table, *mtab;

    PxMount::Tab_LoadFromFile(&mtab, "/etc/mtab").assert("PxFilesystem::main");
    auto mtablist = PxMount::Tab_List(mtab).assert("PxFilesystem::main");
    PxMount::Tab_Destroy(mtab);
    
    PxMount::Tab_LoadFromFile(&table, "/etc/fstab").assert("PxFilesystem::main");
    auto list = PxMount::Tab_List(table).assert("PxFilesystem::main");
    PxMount::Tab_Destroy(table);

    std::set<std::string> skip_devices;
    std::set<std::string> existing_targets;
    for (auto &i : mtablist) {
        skip_devices.insert(i.source);
        existing_targets.insert(i.target);
    }
    
    // perform a filesystem check on each filesystem
    bool failed = false;
    std::cout << "fsck pass 1\n";
    failed |= chkpass(list, 1, skip_devices);
    std::cout << "fsck pass 2\n";
    failed |= chkpass(list, 2, skip_devices);
    if (failed) {
        std::cout << "Failed to check all devices.\n";
        return 1;
    }
    std::cout << "fs mount\n";

    for (auto &i : list) {
        if (existing_targets.count(i.target) != 0) {
            if (i.options.empty())
                i.options = "remount";
            else
                i.options += ",remount";
            std::cout << "target exists\n";
        }
        auto opts = PxFunction::split(i.options, ",");

        bool automount = true;

        for (auto option : opts) {
            if (option == "noauto") {
                automount = false;
            }
        }

        if (automount) {
            std::cout << "\x1B]TSmount " << i.source << " " << i.target << "\x1B]TE"
                << "mount " << i.source << " to "<< i.target << "\n";
            auto mstat = PxMount::Mount(i.source, i.target, i.type, i.options);
            if (mstat.eno != 0) {
                errno = mstat.eno;
				std::perror(("PxFilesystem / on "+i.target+" / " + mstat.funcName).c_str());
                failed = true;
            }
            std::cout << "\x1B]TSmount-end " << i.source << " " << i.target << "\x1B]TE"
                << "finished mounting " << i.source << " to "<< i.target << "\n";
        }
    }
    if (failed) return 1;
    else return 0;
}