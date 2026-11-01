// Kontrol-ID native transparent identity plugin.
//
// Resolves a client IP through a persistent pool of package-owned identity
// helpers, then maps the returned username or source IP to an E2Guardian
// filter group.  Maps are refreshed at most once per second so policy changes
// do not require an engine restart.

#ifdef HAVE_CONFIG_H
#include "e2config.h"
#endif

#include "../Auth.hpp"
#include "../NaughtyFilter.hpp"
#include "../OptionContainer.hpp"
#include "../StoryBoard.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <poll.h>
#include <spawn.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <arpa/inet.h>

extern bool is_daemonised;
extern OptionContainer o;
extern thread_local std::string thread_id;
extern char **environ;

namespace {

constexpr int kExceptionGroup = -2;
constexpr int kBannedGroup = -3;

thread_local std::string current_client_ip;

struct helper_process {
    helper_process() : pid(-1), input_fd(-1), output_fd(-1) {}
    std::mutex lock;
    pid_t pid;
    int input_fd;
    int output_fd;
};

struct network_entry {
    uint32_t network;
    uint32_t mask;
    int prefix;
    int group;
};

struct range_entry {
    uint32_t first;
    uint32_t last;
    int group;
};

struct map_snapshot {
    std::unordered_map<uint32_t, int> exact;
    std::vector<network_entry> networks;
    std::vector<range_entry> ranges;
    std::unordered_map<std::string, int> users;
};

std::string trim_copy(const std::string &value)
{
    std::string::size_type first = 0;
    std::string::size_type last = value.size();
    while (first < last && std::isspace(static_cast<unsigned char>(value[first])))
        ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])))
        --last;
    return value.substr(first, last - first);
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool parse_ipv4(const std::string &value, uint32_t &address)
{
    struct in_addr parsed;
    if (inet_pton(AF_INET, value.c_str(), &parsed) != 1)
        return false;
    address = ntohl(parsed.s_addr);
    return true;
}

int parse_group(std::string value)
{
    const std::string::size_type comment = value.find('#');
    if (comment != std::string::npos)
        value.erase(comment);
    value = lower_copy(trim_copy(value));
    if (value == "exceptioniplist" || value == "exceptionlist" ||
        value == "exceptionip")
        return kExceptionGroup;
    if (value == "bannediplist" || value == "bannedlist" ||
        value == "bannedip")
        return kBannedGroup;

    std::string digits;
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
        if (std::isdigit(static_cast<unsigned char>(*it)))
            digits.push_back(*it);
        else if (!digits.empty())
            break;
    }
    if (digits.empty())
        return -1;
    const int group = std::atoi(digits.c_str());
    return (group >= 1 && group <= o.filter_groups) ? group - 1 : -1;
}

bool parse_ip_entry(const std::string &key, int group, map_snapshot &snapshot)
{
    const std::string::size_type dash = key.find('-');
    if (dash != std::string::npos) {
        uint32_t first = 0;
        uint32_t last = 0;
        if (!parse_ipv4(trim_copy(key.substr(0, dash)), first) ||
            !parse_ipv4(trim_copy(key.substr(dash + 1)), last) || first > last)
            return false;
        snapshot.ranges.push_back(range_entry{first, last, group});
        return true;
    }

    const std::string::size_type slash = key.find('/');
    if (slash == std::string::npos) {
        uint32_t address = 0;
        if (!parse_ipv4(trim_copy(key), address))
            return false;
        snapshot.exact[address] = group;
        return true;
    }

    uint32_t address = 0;
    if (!parse_ipv4(trim_copy(key.substr(0, slash)), address))
        return false;
    const std::string mask_value = trim_copy(key.substr(slash + 1));
    uint32_t mask = 0;
    int prefix = -1;
    if (mask_value.find('.') != std::string::npos) {
        if (!parse_ipv4(mask_value, mask))
            return false;
        prefix = __builtin_popcount(mask);
    } else {
        prefix = std::atoi(mask_value.c_str());
        if (prefix < 0 || prefix > 32)
            return false;
        mask = prefix == 0 ? 0 : (0xffffffffu << (32 - prefix));
    }
    snapshot.networks.push_back(network_entry{address & mask, mask, prefix, group});
    return true;
}

void read_ip_map(const std::string &path, map_snapshot &snapshot)
{
    std::ifstream input(path.c_str());
    std::string line;
    while (std::getline(input, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#')
            continue;
        const std::string::size_type separator = line.find('=');
        if (separator == std::string::npos)
            continue;
        const std::string key = trim_copy(line.substr(0, separator));
        const int group = parse_group(line.substr(separator + 1));
        if (group == -1 || !parse_ip_entry(key, group, snapshot))
            syslog(LOG_WARNING, "Kontrol-ID identity: ignored invalid IP map entry: %s",
                line.c_str());
    }
    std::sort(snapshot.networks.begin(), snapshot.networks.end(),
        [](const network_entry &left, const network_entry &right) {
            return left.prefix > right.prefix;
        });
}

void read_user_map(const std::string &path, map_snapshot &snapshot)
{
    std::ifstream input(path.c_str());
    std::string line;
    while (std::getline(input, line)) {
        line = trim_copy(line);
        if (line.empty() || line[0] == '#')
            continue;
        const std::string::size_type separator = line.find('=');
        if (separator == std::string::npos)
            continue;
        const std::string username = lower_copy(normalise_auth_username(
            trim_copy(line.substr(0, separator))));
        const int group = parse_group(line.substr(separator + 1));
        if (!username.empty() && group >= 0)
            snapshot.users[username] = group;
    }
}

std::string percent_decode(const std::string &value)
{
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size() &&
            std::isxdigit(static_cast<unsigned char>(value[index + 1])) &&
            std::isxdigit(static_cast<unsigned char>(value[index + 2]))) {
            const std::string hex = value.substr(index + 1, 2);
            decoded.push_back(static_cast<char>(std::strtoul(hex.c_str(), NULL, 16)));
            index += 2;
        } else {
            decoded.push_back(value[index]);
        }
    }
    return decoded;
}

std::string response_field(const std::string &response, const std::string &name)
{
    const std::string marker = name + "=";
    const std::string::size_type start = response.find(marker);
    if (start == std::string::npos)
        return std::string();
    const std::string::size_type value_start = start + marker.size();
    const std::string::size_type end = response.find(' ', value_start);
    return percent_decode(response.substr(value_start,
        end == std::string::npos ? std::string::npos : end - value_start));
}

} // namespace

class kontrol_identity_instance : public AuthPlugin
{
public:
    explicit kontrol_identity_instance(ConfigVar &definition)
        : AuthPlugin(definition), next_helper(0), timeout_ms(5000),
          last_map_refresh(0)
    {
        is_connection_based = true;
        needs_proxy_query = false;
        needs_proxy_access_in_plugin = false;
        client_ip_based = true;
    }

    ~kontrol_identity_instance() override { quit(); }

    int init(void *args) override;
    int quit() override;
    int identify(Socket &peercon, Socket &proxycon, HTTPHeader &header,
        std::string &username, bool &is_real_user, auth_rec &authrec) override;
    int determineGroup(std::string &user, int &filter_group,
        StoryBoard &story, NaughtyFilter &check) override;

private:
    bool start_helper(helper_process &helper);
    void stop_helper(helper_process &helper);
    bool exchange(helper_process &helper, const std::string &request,
        std::string &response);
    bool write_all(int fd, const std::string &value);
    bool read_line(int fd, std::string &value);
    void refresh_maps(bool force);

    std::vector<std::unique_ptr<helper_process> > helpers;
    std::atomic<unsigned int> next_helper;
    std::string helper_program;
    std::string helper_config;
    std::string ipgroups_path;
    std::string usergroups_path;
    std::string unknown_user;
    int timeout_ms;
    std::atomic<long long> last_map_refresh;
    std::mutex map_refresh_lock;
    std::shared_ptr<const map_snapshot> maps;
};

AuthPlugin *kontrol_identity_create(ConfigVar &definition)
{
    return new kontrol_identity_instance(definition);
}

bool kontrol_identity_instance::start_helper(helper_process &helper)
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
        const_cast<char *>("-c"),
        const_cast<char *>(helper_config.c_str()),
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
        syslog(LOG_ERR, "Kontrol-ID identity: unable to start helper (%s)",
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

void kontrol_identity_instance::stop_helper(helper_process &helper)
{
    if (helper.input_fd >= 0) {
        close(helper.input_fd);
        helper.input_fd = -1;
    }
    if (helper.output_fd >= 0) {
        close(helper.output_fd);
        helper.output_fd = -1;
    }
    if (helper.pid <= 0)
        return;
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

bool kontrol_identity_instance::write_all(int fd, const std::string &value)
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

bool kontrol_identity_instance::read_line(int fd, std::string &value)
{
    value.clear();
    while (value.size() < 4096) {
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

bool kontrol_identity_instance::exchange(helper_process &helper,
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

void kontrol_identity_instance::refresh_maps(bool force)
{
    const long long now = static_cast<long long>(time(NULL));
    if (!force && last_map_refresh.load() == now)
        return;
    std::lock_guard<std::mutex> guard(map_refresh_lock);
    if (!force && last_map_refresh.load() == now)
        return;

    std::shared_ptr<map_snapshot> next(new map_snapshot());
    read_ip_map(ipgroups_path, *next);
    read_user_map(usergroups_path, *next);
    std::atomic_store(&maps, std::shared_ptr<const map_snapshot>(next));
    last_map_refresh.store(now);
}

int kontrol_identity_instance::init(void *args)
{
    (void)args;
    helper_program = cv["helper_program"].toCharArray();
    if (helper_program.empty())
        helper_program = "/usr/local/libexec/kontrol-identity-helper";
    helper_config = cv["helper_config"].toCharArray();
    if (helper_config.empty())
        helper_config = "/usr/local/etc/kontrol-identity.conf";
    if (access(helper_program.c_str(), X_OK) != 0 ||
        access(helper_config.c_str(), R_OK) != 0) {
        syslog(LOG_ERR, "Kontrol-ID identity: helper or configuration is unavailable");
        return -1;
    }

    ipgroups_path = cv["ipgroups"].toCharArray();
    if (ipgroups_path.empty())
        ipgroups_path = "/usr/local/etc/e2guardian/lists/authplugins/ipgroups";
    usergroups_path = cv["usergroups"].toCharArray();
    if (usergroups_path.empty())
        usergroups_path = "/usr/local/etc/e2guardian/lists/authplugins/filtergroupslist";
    unknown_user = cv["unknown_user"].toCharArray();
    if (unknown_user.empty())
        unknown_user = "unknown";

    int helper_count = cv["helper_children"].toInteger();
    if (helper_count < 1)
        helper_count = 32;
    if (helper_count > 256)
        helper_count = 256;
    timeout_ms = cv["helper_timeout_ms"].toInteger();
    if (timeout_ms < 1000 || timeout_ms > 60000)
        timeout_ms = 5000;
    for (int index = 0; index < helper_count; ++index)
        helpers.push_back(std::unique_ptr<helper_process>(new helper_process()));

    OptionContainer::auth_entry entry;
    entry.entry_function = cv["story_function"];
    if (entry.entry_function.length() < 1) {
        syslog(LOG_ERR, "Kontrol-ID identity: story_function is not configured");
        return -1;
    }
    entry.entry_id = ENT_STORYA_AUTH_IP;
    story_entry = entry.entry_id;
    o.auth_entry_dq.push_back(entry);
    read_def_fg();
    refresh_maps(true);
    return 0;
}

int kontrol_identity_instance::quit()
{
    for (size_t index = 0; index < helpers.size(); ++index) {
        helper_process &helper = *helpers[index];
        std::lock_guard<std::mutex> guard(helper.lock);
        stop_helper(helper);
    }
    helpers.clear();
    return 0;
}

int kontrol_identity_instance::identify(Socket &peercon, Socket &proxycon,
    HTTPHeader &header, std::string &username, bool &is_real_user,
    auth_rec &authrec)
{
    (void)proxycon;
    current_client_ip = trim_copy(header.getClientIP());
    uint32_t parsed = 0;
    if (!parse_ipv4(current_client_ip, parsed))
        current_client_ip = trim_copy(peercon.getPeerIP());
    if (!parse_ipv4(current_client_ip, parsed))
        return E2AUTH_NOMATCH;

    username = unknown_user;
    std::string source = "helper-unavailable";
    if (!helpers.empty()) {
        const size_t first = next_helper.fetch_add(1) % helpers.size();
        helper_process *selected = NULL;
        for (size_t offset = 0; offset < helpers.size(); ++offset) {
            helper_process &candidate = *helpers[(first + offset) % helpers.size()];
            if (candidate.lock.try_lock()) {
                selected = &candidate;
                break;
            }
        }
        if (selected == NULL) {
            selected = helpers[first].get();
            selected->lock.lock();
        }
        std::unique_lock<std::mutex> guard(selected->lock, std::adopt_lock);
        std::string response;
        if (exchange(*selected, current_client_ip, response) &&
            response.compare(0, 2, "OK") == 0) {
            const std::string resolved = response_field(response, "user");
            const std::string tag = response_field(response, "tag");
            if (!resolved.empty())
                username = resolved;
            if (!tag.empty())
                source = tag;
        } else {
            syslog(LOG_ERR, "Kontrol-ID identity: helper lookup failed for %s",
                current_client_ip.c_str());
        }
    }

    if (lower_copy(username) != lower_copy(unknown_user))
        username = normalise_auth_username(username);
    if (username.empty())
        username = unknown_user;
    authrec.user_name = username;
    authrec.user_source = source;
    authrec.is_authed = lower_copy(username) != lower_copy(unknown_user);
    is_real_user = true;
    return E2AUTH_OK;
}

int kontrol_identity_instance::determineGroup(std::string &user,
    int &filter_group, StoryBoard &story, NaughtyFilter &check)
{
    (void)story;
    refresh_maps(false);
    const std::shared_ptr<const map_snapshot> snapshot = std::atomic_load(&maps);
    if (!snapshot)
        return E2AUTH_NOMATCH;

    int group = -1;
    std::string source;
    const std::string normalized_user = lower_copy(normalise_auth_username(user));
    if (!normalized_user.empty() && normalized_user != lower_copy(unknown_user)) {
        const std::unordered_map<std::string, int>::const_iterator found =
            snapshot->users.find(normalized_user);
        if (found != snapshot->users.end()) {
            group = found->second;
            source = "identity";
        }
    }

    uint32_t address = 0;
    if (group < 0 && parse_ipv4(current_client_ip, address)) {
        const std::unordered_map<uint32_t, int>::const_iterator exact =
            snapshot->exact.find(address);
        if (exact != snapshot->exact.end()) {
            group = exact->second;
        } else {
            for (std::vector<network_entry>::const_iterator it =
                    snapshot->networks.begin(); it != snapshot->networks.end(); ++it) {
                if ((address & it->mask) == it->network) {
                    group = it->group;
                    break;
                }
            }
            if (group < 0) {
                for (std::vector<range_entry>::const_iterator it =
                        snapshot->ranges.begin(); it != snapshot->ranges.end(); ++it) {
                    if (address >= it->first && address <= it->last) {
                        group = it->group;
                        break;
                    }
                }
            }
        }
        if (group != -1)
            source = "ip";
    }

    if (group == kExceptionGroup || group == kBannedGroup) {
        check.filtergroup = filter_group;
        if (group == kExceptionGroup) {
            check.isexception = true;
            check.isException = true;
            check.special_exception_ip = true;
            check.nomitm = true;
            check.automitm = false;
        } else {
            check.isBlocked = true;
            check.isItNaughty = true;
            check.special_banned_ip = true;
        }
        return E2AUTH_NOGROUP;
    }

    if (group < 0) {
        const int configured_default = get_default(!check.request_header->isProxyRequest);
        if (configured_default > 0) {
            group = configured_default - 1;
            source = "default";
        }
    }
    if (group < 0)
        return E2AUTH_NOGROUP;

    filter_group = group;
    check.filtergroup = group;
    check.user = user;
    if (check.authrec != NULL) {
        check.authrec->filter_group = group;
        check.authrec->group_source = source;
        if (check.authrec->user_name.length() == 0)
            check.authrec->user_name = user;
    }
    return E2AUTH_OK;
}
