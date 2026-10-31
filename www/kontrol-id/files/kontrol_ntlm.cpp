// Kontrol-ID native NTLMv2 authentication plugin.
//
// Completes the connection-oriented NTLM proxy handshake directly on the
// E2Guardian listener and delegates cryptographic verification to Samba's
// ntlm_auth helper over its squid-2.5-ntlmssp protocol.

#ifdef HAVE_CONFIG_H
#include "e2config.h"
#endif

#include "../Auth.hpp"
#include "../NaughtyFilter.hpp"
#include "../OptionContainer.hpp"
#include "../StoryBoard.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <poll.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <vector>

extern bool is_daemonised;
extern OptionContainer o;
extern thread_local std::string thread_id;
extern char **environ;

namespace {

struct helper_process {
    helper_process() : pid(-1), input_fd(-1), output_fd(-1) {}

    std::mutex lock;
    pid_t pid;
    int input_fd;
    int output_fd;
};

bool is_ntlm(HTTPHeader &header)
{
    String type(header.getAuthType());
    type.toLower();
    return type == "ntlm";
}

bool send_challenge(Socket &peercon, const std::string &token)
{
    std::string response =
        "HTTP/1.1 407 Proxy Authentication Required\r\n"
        "Proxy-Authenticate: NTLM";
    if (!token.empty()) {
        response += " ";
        response += token;
    }
    response +=
        "\r\nProxy-Connection: keep-alive"
        "\r\nConnection: keep-alive"
        "\r\nContent-Length: 0"
        "\r\nCache-Control: no-store"
        "\r\n\r\n";
    return peercon.writeString(response);
}

std::string trim_copy(const std::string &value)
{
    const std::string whitespace(" \t\r\n");
    const std::string::size_type begin = value.find_first_not_of(whitespace);
    if (begin == std::string::npos)
        return std::string();
    const std::string::size_type end = value.find_last_not_of(whitespace);
    return value.substr(begin, end - begin + 1);
}

} // namespace

class kontrol_ntlm_instance : public AuthPlugin
{
public:
    explicit kontrol_ntlm_instance(ConfigVar &definition)
        : AuthPlugin(definition), next_helper(0), timeout_ms(10000)
    {
        is_connection_based = true;
        needs_proxy_query = false;
        needs_proxy_access_in_plugin = false;
        client_ip_based = false;
    }

    ~kontrol_ntlm_instance() override
    {
        quit();
    }

    int init(void *args) override;
    int quit() override;
    int identify(Socket &peercon, Socket &proxycon, HTTPHeader &header,
        std::string &username, bool &is_real_user, auth_rec &authrec) override;

private:
    bool start_helper(helper_process &helper);
    void stop_helper(helper_process &helper);
    bool exchange(helper_process &helper, const std::string &request,
        std::string &response);
    bool write_all(int fd, const std::string &value);
    bool read_line(int fd, std::string &value);

    std::vector<std::unique_ptr<helper_process> > helpers;
    std::atomic<unsigned int> next_helper;
    std::string helper_program;
    std::string samba_config;
    int timeout_ms;
};

AuthPlugin *kontrol_ntlm_create(ConfigVar &definition)
{
    return new kontrol_ntlm_instance(definition);
}

bool kontrol_ntlm_instance::start_helper(helper_process &helper)
{
    if (helper.pid > 0 && helper.input_fd >= 0 && helper.output_fd >= 0)
        return true;

    int input_pipe[2] = {-1, -1};
    int output_pipe[2] = {-1, -1};
    if (pipe(input_pipe) != 0 || pipe(output_pipe) != 0) {
        if (input_pipe[0] >= 0) close(input_pipe[0]);
        if (input_pipe[1] >= 0) close(input_pipe[1]);
        if (output_pipe[0] >= 0) close(output_pipe[0]);
        if (output_pipe[1] >= 0) close(output_pipe[1]);
        syslog(LOG_ERR, "Kontrol-ID NTLM: unable to create helper pipes");
        return false;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, input_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, output_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, input_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, output_pipe[0]);
    /* The gateway has listening sockets and accepted client connections open
     * while helpers are spawned.  Keep only stdin/stdout in ntlm_auth so a
     * helper can never retain a listener across an engine restart. */
    posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1);

    const std::string config_argument = "--configfile=" + samba_config;
    const std::string protocol_argument = "--helper-protocol=squid-2.5-ntlmssp";
    char *argv[] = {
        const_cast<char *>(helper_program.c_str()),
        const_cast<char *>(config_argument.c_str()),
        const_cast<char *>(protocol_argument.c_str()),
        NULL
    };
    pid_t child = -1;
    const int result = posix_spawn(&child, helper_program.c_str(), &actions,
        NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(input_pipe[0]);
    close(output_pipe[1]);
    if (result != 0) {
        close(input_pipe[1]);
        close(output_pipe[0]);
        syslog(LOG_ERR, "Kontrol-ID NTLM: unable to start ntlm_auth (%s)",
            strerror(result));
        return false;
    }

    fcntl(input_pipe[1], F_SETFD, FD_CLOEXEC);
    fcntl(output_pipe[0], F_SETFD, FD_CLOEXEC);
    helper.pid = child;
    helper.input_fd = input_pipe[1];
    helper.output_fd = output_pipe[0];
    return true;
}

void kontrol_ntlm_instance::stop_helper(helper_process &helper)
{
    if (helper.input_fd >= 0) {
        close(helper.input_fd);
        helper.input_fd = -1;
    }
    if (helper.output_fd >= 0) {
        close(helper.output_fd);
        helper.output_fd = -1;
    }
    if (helper.pid > 0) {
        kill(helper.pid, SIGTERM);
        int status = 0;
        for (unsigned int attempt = 0; attempt < 20; ++attempt) {
            const pid_t waited = waitpid(helper.pid, &status, WNOHANG);
            if (waited == helper.pid || (waited < 0 && errno == ECHILD)) {
                helper.pid = -1;
                return;
            }
            usleep(10000);
        }
        kill(helper.pid, SIGKILL);
        waitpid(helper.pid, &status, 0);
        helper.pid = -1;
    }
}

bool kontrol_ntlm_instance::write_all(int fd, const std::string &value)
{
    size_t offset = 0;
    while (offset < value.size()) {
        const ssize_t written = write(fd, value.data() + offset,
            value.size() - offset);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return false;
        offset += static_cast<size_t>(written);
    }
    return true;
}

bool kontrol_ntlm_instance::read_line(int fd, std::string &value)
{
    value.clear();
    while (value.size() < 65536) {
        struct pollfd descriptor;
        descriptor.fd = fd;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        int ready;
        do {
            ready = poll(&descriptor, 1, timeout_ms);
        } while (ready < 0 && errno == EINTR);
        if (ready <= 0 || !(descriptor.revents & (POLLIN | POLLHUP)))
            return false;

        char ch = 0;
        const ssize_t count = read(fd, &ch, 1);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return false;
        if (ch == '\n')
            return true;
        if (ch != '\r')
            value.push_back(ch);
    }
    return false;
}

bool kontrol_ntlm_instance::exchange(helper_process &helper,
    const std::string &request, std::string &response)
{
    if (!start_helper(helper))
        return false;
    if (!write_all(helper.input_fd, request + "\n") ||
        !read_line(helper.output_fd, response)) {
        stop_helper(helper);
        return false;
    }
    return true;
}

int kontrol_ntlm_instance::init(void *args)
{
    (void)args;
    helper_program = cv["helper_program"].toCharArray();
    samba_config = cv["samba_config"].toCharArray();
    if (helper_program.empty())
        helper_program = "/usr/local/bin/ntlm_auth";
    if (samba_config.empty())
        samba_config = "/usr/local/etc/kontrol-id/smb4.conf";
    if (access(helper_program.c_str(), X_OK) != 0 ||
        access(samba_config.c_str(), R_OK) != 0) {
        syslog(LOG_ERR, "Kontrol-ID NTLM: helper or Samba configuration is unavailable");
        return -1;
    }

    int helper_count = cv["helper_children"].toInteger();
    if (helper_count < 1)
        helper_count = 32;
    if (helper_count > 256)
        helper_count = 256;
    timeout_ms = cv["helper_timeout_ms"].toInteger();
    if (timeout_ms < 1000 || timeout_ms > 60000)
        timeout_ms = 10000;
    for (int index = 0; index < helper_count; ++index)
        helpers.push_back(std::unique_ptr<helper_process>(new helper_process()));

    OptionContainer::auth_entry entry;
    entry.entry_function = cv["story_function"];
    if (entry.entry_function.length() < 1) {
        syslog(LOG_ERR, "Kontrol-ID NTLM: story_function is not configured");
        return -1;
    }
    entry.entry_id = ENT_STORYA_AUTH_PF_BASIC;
    story_entry = entry.entry_id;
    o.auth_entry_dq.push_back(entry);
    read_def_fg();
    return 0;
}

int kontrol_ntlm_instance::quit()
{
    for (size_t index = 0; index < helpers.size(); ++index) {
        helper_process &helper = *helpers[index];
        std::lock_guard<std::mutex> guard(helper.lock);
        stop_helper(helper);
    }
    helpers.clear();
    return 0;
}

int kontrol_ntlm_instance::identify(Socket &peercon, Socket &proxycon,
    HTTPHeader &header, std::string &username, bool &is_real_user,
    auth_rec &authrec)
{
    (void)proxycon;
    if (!header.isProxyRequest || helpers.empty())
        return E2AUTH_NOMATCH;

    if (!is_ntlm(header) || header.getRawAuthData().empty()) {
        if (!send_challenge(peercon, std::string()) ||
            !header.in(&peercon, true))
            return -1;
    }
    if (!is_ntlm(header))
        return -1;

    std::string type1 = trim_copy(header.getRawAuthData());
    if (type1.empty() || type1.size() > 32768)
        return -1;

    helper_process &helper = *helpers[next_helper.fetch_add(1) % helpers.size()];
    std::unique_lock<std::mutex> guard(helper.lock);
    std::string response;
    if (!exchange(helper, "YR " + type1, response) ||
        response.compare(0, 3, "TT ") != 0) {
        syslog(LOG_ERR, "Kontrol-ID NTLM: helper did not produce a challenge");
        stop_helper(helper);
        send_challenge(peercon, std::string());
        return -1;
    }

    const std::string challenge = trim_copy(response.substr(3));
    if (challenge.empty() || !send_challenge(peercon, challenge) ||
        !header.in(&peercon, true) || !is_ntlm(header)) {
        stop_helper(helper);
        return -1;
    }

    const std::string type3 = trim_copy(header.getRawAuthData());
    if (type3.empty() || type3.size() > 65536 ||
        !exchange(helper, "KK " + type3, response)) {
        stop_helper(helper);
        return -1;
    }
    if (response.compare(0, 3, "AF ") != 0) {
        // NA is an authentication rejection and BH is a helper failure.  Both
        // fail closed without logging tokens or credential-derived material.
        if (response.compare(0, 2, "BH") == 0)
            stop_helper(helper);
        send_challenge(peercon, std::string());
        return -1;
    }

    username = normalise_auth_username(trim_copy(response.substr(3)));
    if (username.empty())
        return -1;
    authrec.user_name = username;
    authrec.user_source = "kontrol-ntlm";
    authrec.is_authed = true;
    is_real_user = true;
    return E2AUTH_OK;
}
