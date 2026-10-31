// Kontrol-ID native HTTP Basic authentication plugin.
//
// Validates Proxy-Authorization credentials against the selected Kontrol
// authentication server through the package-owned persistent PHP helper.
// Credentials are exchanged only through inherited pipes and are never logged.

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

bool is_basic(HTTPHeader &header)
{
    String type(header.getAuthType());
    type.toLower();
    return type == "basic";
}

std::string percent_encode(const std::string &value)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        const unsigned char ch = static_cast<unsigned char>(*it);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
            ch == '.' || ch == '~') {
            encoded.push_back(static_cast<char>(ch));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[(ch >> 4) & 0x0f]);
            encoded.push_back(hex[ch & 0x0f]);
        }
    }
    return encoded;
}

bool send_challenge(Socket &peercon, const std::string &realm)
{
    const std::string response =
        "HTTP/1.1 407 Proxy Authentication Required\r\n"
        "Proxy-Authenticate: Basic realm=\"" + realm + "\", charset=\"UTF-8\"\r\n"
        "Proxy-Connection: keep-alive\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 0\r\n"
        "Cache-Control: no-store\r\n\r\n";
    return peercon.writeString(response);
}

} // namespace

class kontrol_basic_instance : public AuthPlugin
{
public:
    explicit kontrol_basic_instance(ConfigVar &definition)
        : AuthPlugin(definition), next_helper(0), timeout_ms(10000)
    {
        needs_proxy_query = false;
        needs_proxy_access_in_plugin = false;
        client_ip_based = false;
    }

    ~kontrol_basic_instance() override
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
    std::string realm;
    int timeout_ms;
};

AuthPlugin *kontrol_basic_create(ConfigVar &definition)
{
    return new kontrol_basic_instance(definition);
}

bool kontrol_basic_instance::start_helper(helper_process &helper)
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
        syslog(LOG_ERR, "Kontrol-ID Basic: unable to create helper pipes");
        return false;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, input_pipe[0], STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, output_pipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, input_pipe[1]);
    posix_spawn_file_actions_addclose(&actions, output_pipe[0]);
    posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1);

    char *argv[] = {
        const_cast<char *>(helper_program.c_str()),
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
        syslog(LOG_ERR, "Kontrol-ID Basic: unable to start helper (%s)",
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

void kontrol_basic_instance::stop_helper(helper_process &helper)
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

bool kontrol_basic_instance::write_all(int fd, const std::string &value)
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

bool kontrol_basic_instance::read_line(int fd, std::string &value)
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

bool kontrol_basic_instance::exchange(helper_process &helper,
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

int kontrol_basic_instance::init(void *args)
{
    (void)args;
    helper_program = cv["helper_program"].toCharArray();
    if (helper_program.empty())
        helper_program = "/usr/local/libexec/kontrol-basic-auth";
    if (access(helper_program.c_str(), X_OK) != 0) {
        syslog(LOG_ERR, "Kontrol-ID Basic: authentication helper is unavailable");
        return -1;
    }

    realm = cv["realm"].toCharArray();
    if (realm.empty())
        realm = "Kontrol Internet Access";
    for (std::string::iterator it = realm.begin(); it != realm.end(); ++it) {
        if (*it == '"' || *it == '\\' || *it == '\r' || *it == '\n')
            *it = '_';
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
        syslog(LOG_ERR, "Kontrol-ID Basic: story_function is not configured");
        return -1;
    }
    entry.entry_id = ENT_STORYA_AUTH_PF_BASIC;
    story_entry = entry.entry_id;
    o.auth_entry_dq.push_back(entry);
    read_def_fg();
    return 0;
}

int kontrol_basic_instance::quit()
{
    for (size_t index = 0; index < helpers.size(); ++index) {
        helper_process &helper = *helpers[index];
        std::lock_guard<std::mutex> guard(helper.lock);
        stop_helper(helper);
    }
    helpers.clear();
    return 0;
}

int kontrol_basic_instance::identify(Socket &peercon, Socket &proxycon,
    HTTPHeader &header, std::string &username, bool &is_real_user,
    auth_rec &authrec)
{
    (void)proxycon;
    if (!header.isProxyRequest || helpers.empty())
        return E2AUTH_NOMATCH;

    if (!is_basic(header) || header.getRawAuthData().empty()) {
        if (!send_challenge(peercon, realm) || !header.in(&peercon, true))
            return -1;
    }
    if (!is_basic(header))
        return -1;

    const std::string credentials = header.getAuthData();
    const std::string::size_type separator = credentials.find(':');
    if (separator == std::string::npos || separator == 0 ||
        separator > 255 || credentials.size() - separator - 1 > 4096) {
        send_challenge(peercon, realm);
        return -1;
    }
    const std::string submitted_user = credentials.substr(0, separator);
    const std::string password = credentials.substr(separator + 1);
    if (password.empty()) {
        send_challenge(peercon, realm);
        return -1;
    }

    helper_process &helper = *helpers[next_helper.fetch_add(1) % helpers.size()];
    std::lock_guard<std::mutex> guard(helper.lock);
    std::string response;
    if (!exchange(helper, percent_encode(submitted_user) + " " +
        percent_encode(password), response)) {
        syslog(LOG_ERR, "Kontrol-ID Basic: authentication helper failed");
        send_challenge(peercon, realm);
        return -1;
    }
    if (response.compare(0, 2, "OK") != 0) {
        send_challenge(peercon, realm);
        return -1;
    }

    username = normalise_auth_username(submitted_user);
    if (username.empty())
        return -1;
    authrec.user_name = username;
    authrec.user_source = "kontrol-basic";
    authrec.is_authed = true;
    is_real_user = true;
    return E2AUTH_OK;
}
