
#include <filesystem>
#include <security/pam_appl.h>
#include <cerrno>
#include <bits/types/sigset_t.h>
#include <cstddef>
#include <cstdlib>
#include <security/_pam_types.h>
#include <security/pam_modules.h>
#include <PxIPC.hpp>
#include <PxProcess.hpp>
#include <string>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <pthread.h>
#include <PxEnv.hpp>

#define PAMASSERT(res) { auto _RES = res; if (_RES.eno != 0) return PAM_SESSION_ERR; }

static struct passwd* getpwofpam(pam_handle_t *pamh) {
    // get username
    char *username;
    if (pam_get_item(pamh, PAM_USER, (const void**)&username) != PAM_SUCCESS) {
        return NULL;
    }

    // get user ID of username
    return getpwnam(username);
}

PAM_EXTERN "C" int pam_sm_open_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    auto pwd = getpwofpam(pamh);
    if (pwd == NULL) {
        std::cout << "User does not exist in /etc/passwd, skipping\n";
        return PAM_SESSION_ERR;
    }

    PxIPC::Client usercli;

    auto upath = "/run/parallax-svman-u" + std::to_string(pwd->pw_uid) + ".sock";
    auto ures = usercli.Connect(upath);
    if (ures.eno == ECONNREFUSED || ures.eno == ENOENT) {
        remove(("/run/targets/all-u" + std::to_string(pwd->pw_uid)).c_str());

        if (ures.eno == ECONNREFUSED) {
            auto _ = remove(upath.c_str());
        }
        
        {
            std::string envfile;
            for (int envn = 0; std::filesystem::exists(envfile = "/run/envfile-"+std::to_string(envn)+".env");envn++)
                ;

            PxEnv::dump(envfile);
            PxIPC::Client cli;
            if (cli.Connect("/run/parallax-svman.sock").eno != 0) {
                return PAM_SESSION_ERR;
            }
            if (cli.Write("spawn "+std::to_string(pwd->pw_uid)+" "+std::to_string(pwd->pw_gid)+" "+pwd->pw_dir+" "+envfile+"\0").eno != 0) {
                return PAM_SESSION_ERR;
            }
        }

        while (ures.eno == ECONNREFUSED || ures.eno == ENOENT) {
            usleep(1000);
            ures = usercli.Connect(upath);
        }
    }

    PAMASSERT(ures);
    PAMASSERT(usercli.Write("reference inc\0"));

    // TODO: better
    auto tenv = "/run/targetenv/all-u" + std::to_string(pwd->pw_uid);
    PxFunction::waitExist(tenv);
    std::vector<std::string> strings;
    PxEnv::loadStrings(tenv, strings);
    for (auto &i : strings) {
        pam_putenv(pamh, i.c_str());
    }

    return PAM_SUCCESS;
}
PAM_EXTERN "C" int pam_sm_close_session(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    auto pwd = getpwofpam(pamh);
    if (pwd == NULL) {
        return PAM_SESSION_ERR;
    }

    PxIPC::Client cli;
    PAMASSERT( cli.Connect("/run/parallax-svman-u"+std::to_string(pwd->pw_uid)+".sock") );
    PAMASSERT( cli.Write("reference logout\0") );
    return PAM_SUCCESS;
}