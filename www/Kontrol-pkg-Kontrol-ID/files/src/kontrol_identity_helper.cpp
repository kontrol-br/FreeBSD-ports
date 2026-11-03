/*
 * Kontrol-ID Squid external ACL helper.
 *
 * The process is owned by Squid and accepts one source address per line. It
 * does not listen on a network socket. Endpoint queries use WS-Management over
 * mutual TLS. It reads Win32_ComputerSystem.UserName and, when no console
 * user exists, resolves the first loaded non-special user profile.
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <sqlite3.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char *kVersion = "0.1.5";
constexpr const char *kResourceUri =
    "http://schemas.microsoft.com/wbem/wsman/1/wmi/root/cimv2/Win32_ComputerSystem";

std::string trim(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool parse_bool(const std::string &value, bool fallback) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (v == "1" || v == "yes" || v == "true" || v == "on") return true;
    if (v == "0" || v == "no" || v == "false" || v == "off") return false;
    return fallback;
}

int parse_int(const std::string &value, int fallback, int minimum, int maximum) {
    try {
        const int parsed = std::stoi(value);
        return std::max(minimum, std::min(maximum, parsed));
    } catch (...) {
        return fallback;
    }
}

struct Config {
    bool enabled{false};
    bool verify_peer{true};
    bool verify_host{true};
    bool debug{false};
    int winrm_port{5986};
    int connect_timeout_ms{1200};
    int request_timeout_ms{2500};
    int positive_ttl{300};
    int negative_ttl{30};
    int stale_ttl{900};
    std::string unknown_user{"unknown"};
    std::string database{"/var/db/kontrol-identity/identity.db"};
    std::string ca_file{"/var/etc/kontrol-id/ca.pem"};
    std::string client_cert{"/var/etc/kontrol-id/client.crt"};
    std::string client_key{"/var/etc/kontrol-id/client.key"};

    static Config load(const std::string &path) {
        Config config;
        std::ifstream input(path);
        if (!input) {
            throw std::runtime_error("cannot open configuration: " + path);
        }

        std::map<std::string, std::string> values;
        std::string line;
        while (std::getline(input, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == '[') continue;
            const auto separator = line.find('=');
            if (separator == std::string::npos) continue;
            values[trim(line.substr(0, separator))] = trim(line.substr(separator + 1));
        }

        auto value = [&](const std::string &key, const std::string &fallback) {
            const auto it = values.find(key);
            return it == values.end() ? fallback : it->second;
        };

        config.enabled = parse_bool(value("enabled", "false"), false);
        config.verify_peer = parse_bool(value("verify_peer", "true"), true);
        config.verify_host = parse_bool(value("verify_host", "true"), true);
        config.debug = parse_bool(value("debug", "false"), false);
        config.winrm_port = parse_int(value("winrm_port", "5986"), 5986, 1, 65535);
        config.connect_timeout_ms = parse_int(value("connect_timeout_ms", "1200"), 1200, 200, 30000);
        config.request_timeout_ms = parse_int(value("request_timeout_ms", "2500"), 2500, 500, 60000);
        config.positive_ttl = parse_int(value("positive_ttl", "300"), 300, 5, 86400);
        config.negative_ttl = parse_int(value("negative_ttl", "30"), 30, 1, 3600);
        config.stale_ttl = parse_int(value("stale_ttl", "900"), 900, 0, 86400);
        config.unknown_user = value("unknown_user", "unknown");
        config.database = value("database", config.database);
        config.ca_file = value("ca_file", config.ca_file);
        config.client_cert = value("client_cert", config.client_cert);
        config.client_key = value("client_key", config.client_key);
        return config;
    }
};

struct Identity {
    std::string username;
    std::string source;
    long long expires_at{0};
    long long stale_until{0};
};

struct Endpoint {
    std::string hostname;
    std::string connection_name;
};

long long now_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class Database {
public:
    explicit Database(const std::string &path) {
        if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
            const std::string error = db_ ? sqlite3_errmsg(db_) : "unknown SQLite error";
            throw std::runtime_error("cannot open identity database: " + error);
        }
        sqlite3_busy_timeout(db_, 1000);
        execute("PRAGMA journal_mode=WAL");
        execute("PRAGMA foreign_keys=ON");
        execute("CREATE TABLE IF NOT EXISTS identities ("
                "ip TEXT PRIMARY KEY, username TEXT NOT NULL, source TEXT NOT NULL, "
                "state TEXT NOT NULL, updated_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, "
                "stale_until INTEGER NOT NULL, detail TEXT NOT NULL DEFAULT '')");
        execute("CREATE TABLE IF NOT EXISTS portal_sessions ("
                "ip TEXT PRIMARY KEY, username TEXT NOT NULL, backend TEXT NOT NULL, "
                "created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, revoked INTEGER NOT NULL DEFAULT 0)");
        execute("CREATE TABLE IF NOT EXISTS endpoints ("
                "ip TEXT PRIMARY KEY, hostname TEXT NOT NULL, fqdn TEXT NOT NULL DEFAULT '', "
                "cert_sha256 TEXT NOT NULL DEFAULT '', enrolled_at INTEGER NOT NULL, "
                "last_seen INTEGER NOT NULL DEFAULT 0, revoked INTEGER NOT NULL DEFAULT 0)");
    }

    ~Database() {
        if (db_) sqlite3_close(db_);
    }

    std::optional<Identity> portal(const std::string &ip, long long now) {
        sqlite3_stmt *statement = nullptr;
        const char *sql = "SELECT username, backend, expires_at FROM portal_sessions "
                          "WHERE ip=?1 AND revoked=0 AND expires_at>?2";
        prepare(sql, &statement);
        sqlite3_bind_text(statement, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement, 2, now);
        std::optional<Identity> result;
        if (sqlite3_step(statement) == SQLITE_ROW) {
            result = Identity{column(statement, 0), "portal-" + column(statement, 1),
                              sqlite3_column_int64(statement, 2), sqlite3_column_int64(statement, 2)};
        }
        sqlite3_finalize(statement);
        return result;
    }

    std::optional<Identity> cached(const std::string &ip) {
        sqlite3_stmt *statement = nullptr;
        prepare("SELECT username, source, expires_at, stale_until FROM identities WHERE ip=?1", &statement);
        sqlite3_bind_text(statement, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        std::optional<Identity> result;
        if (sqlite3_step(statement) == SQLITE_ROW) {
            result = Identity{column(statement, 0), column(statement, 1),
                              sqlite3_column_int64(statement, 2), sqlite3_column_int64(statement, 3)};
        }
        sqlite3_finalize(statement);
        return result;
    }

    std::optional<Endpoint> endpoint(const std::string &ip) {
        sqlite3_stmt *statement = nullptr;
        prepare("SELECT hostname,CASE WHEN fqdn<>'' THEN fqdn ELSE hostname END FROM endpoints "
                "WHERE ip=?1 AND revoked=0", &statement);
        sqlite3_bind_text(statement, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        std::optional<Endpoint> result;
        if (sqlite3_step(statement) == SQLITE_ROW) {
            result = Endpoint{column(statement, 0), column(statement, 1)};
        }
        sqlite3_finalize(statement);
        return result;
    }

    void store(const std::string &ip, const Identity &identity, const std::string &state,
               const std::string &detail) {
        sqlite3_stmt *statement = nullptr;
        prepare("INSERT INTO identities(ip,username,source,state,updated_at,expires_at,stale_until,detail) "
                "VALUES(?1,?2,?3,?4,?5,?6,?7,?8) "
                "ON CONFLICT(ip) DO UPDATE SET username=excluded.username,source=excluded.source,"
                "state=excluded.state,updated_at=excluded.updated_at,expires_at=excluded.expires_at,"
                "stale_until=excluded.stale_until,detail=excluded.detail", &statement);
        sqlite3_bind_text(statement, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, identity.username.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 3, identity.source.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 4, state.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement, 5, now_seconds());
        sqlite3_bind_int64(statement, 6, identity.expires_at);
        sqlite3_bind_int64(statement, 7, identity.stale_until);
        sqlite3_bind_text(statement, 8, detail.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(statement) != SQLITE_DONE) {
            syslog(LOG_WARNING, "Kontrol-ID: unable to update cache: %s", sqlite3_errmsg(db_));
        }
        sqlite3_finalize(statement);
    }

    void touch_endpoint(const std::string &ip, long long now) {
        sqlite3_stmt *statement = nullptr;
        prepare("UPDATE endpoints SET last_seen=?2 WHERE ip=?1", &statement);
        sqlite3_bind_text(statement, 1, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement, 2, now);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
    }

private:
    sqlite3 *db_{nullptr};

    void execute(const char *sql) {
        char *error = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
            const std::string message = error ? error : "unknown SQLite error";
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }

    void prepare(const char *sql, sqlite3_stmt **statement) {
        if (sqlite3_prepare_v2(db_, sql, -1, statement, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db_));
        }
    }

    static std::string column(sqlite3_stmt *statement, int index) {
        const unsigned char *value = sqlite3_column_text(statement, index);
        return value ? reinterpret_cast<const char *>(value) : "";
    }
};

std::string xml_escape(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (const char c : value) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '\"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += c;
        }
    }
    return result;
}

std::string message_id() {
    std::random_device random;
    std::uniform_int_distribution<unsigned int> byte(0, 255);
    unsigned char data[16];
    for (auto &value : data) value = static_cast<unsigned char>(byte(random));
    data[6] = static_cast<unsigned char>((data[6] & 0x0f) | 0x40);
    data[8] = static_cast<unsigned char>((data[8] & 0x3f) | 0x80);
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) output << '-';
        output << std::setw(2) << static_cast<int>(data[i]);
    }
    return output.str();
}

size_t curl_write(char *data, size_t size, size_t count, void *context) {
    auto *body = static_cast<std::string *>(context);
    body->append(data, size * count);
    return size * count;
}

std::string xpath_text(const std::string &document, const char *expression) {
    xmlDocPtr parsed = xmlReadMemory(document.data(), static_cast<int>(document.size()),
                                     "winrm.xml", nullptr,
                                     XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!parsed) return "";
    xmlXPathContextPtr context = xmlXPathNewContext(parsed);
    xmlXPathObjectPtr object = context ? xmlXPathEvalExpression(
        reinterpret_cast<const xmlChar *>(expression), context) : nullptr;
    std::string result;
    if (object && object->nodesetval && object->nodesetval->nodeNr > 0) {
        xmlChar *content = xmlNodeGetContent(object->nodesetval->nodeTab[0]);
        if (content) {
            result = reinterpret_cast<const char *>(content);
            xmlFree(content);
        }
    }
    if (object) xmlXPathFreeObject(object);
    if (context) xmlXPathFreeContext(context);
    xmlFreeDoc(parsed);
    return trim(result);
}

bool valid_source_address(const std::string &address) {
    unsigned char buffer[sizeof(struct in6_addr)];
    return inet_pton(AF_INET, address.c_str(), buffer) == 1 ||
           inet_pton(AF_INET6, address.c_str(), buffer) == 1;
}

bool valid_hostname(const std::string &hostname) {
    if (hostname.empty() || hostname.size() > 253) return false;
    return std::all_of(hostname.begin(), hostname.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-';
    });
}

struct WinrmResult {
    std::string username;
    std::string error;
    bool no_interactive_user{false};
};

struct WinrmHttpResult {
    long status{0};
    std::string body;
    std::string error;
};

/* The engine keeps a pool of helper processes.  When a cached identity
 * expires, a busy browser can send the same source address to every child at
 * once.  Without a per-endpoint lock those children all open WinRM sessions;
 * a late timeout can then overwrite a successful user lookup with the
 * hostname fallback.  Serialize only lookups for the same IP so unrelated
 * workstations remain fully parallel. */
class EndpointLookupLock {
public:
    EndpointLookupLock(const std::string &database, const std::string &ip) {
        const std::string::size_type separator = database.find_last_of('/');
        const std::string parent = separator == std::string::npos ? "." :
            (separator == 0 ? "/" : database.substr(0, separator));
        const std::string directory = parent + "/locks";
        if (::mkdir(directory.c_str(), 0770) != 0 && errno != EEXIST) {
            throw std::runtime_error("cannot create identity lock directory");
        }
        struct stat directory_stat {};
        if (::lstat(directory.c_str(), &directory_stat) != 0 ||
            !S_ISDIR(directory_stat.st_mode)) {
            throw std::runtime_error("identity lock path is not a directory");
        }

        std::uint64_t hash = 1469598103934665603ULL;
        for (const unsigned char c : ip) {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        std::ostringstream name;
        name << directory << "/endpoint-" << std::hex << hash << ".lock";
        fd_ = ::open(name.str().c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0660);
        if (fd_ < 0) {
            throw std::runtime_error("cannot open identity endpoint lock");
        }
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(250);
        while (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
            const int saved_errno = errno;
            if (saved_errno == EWOULDBLOCK) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    ::close(fd_);
                    fd_ = -1;
                    return;
                }
                ::usleep(10000);
                continue;
            }
            ::close(fd_);
            fd_ = -1;
            errno = saved_errno;
            throw std::runtime_error("cannot acquire identity endpoint lock");
        }
    }

    EndpointLookupLock(const EndpointLookupLock &) = delete;
    EndpointLookupLock &operator=(const EndpointLookupLock &) = delete;

    bool acquired() const { return fd_ >= 0; }

    ~EndpointLookupLock() {
        if (fd_ >= 0) {
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
        }
    }

private:
    int fd_{-1};
};

WinrmHttpResult winrm_post(const Config &config, const std::string &ip,
                           const std::string &hostname, const std::string &url,
                           const std::string &soap) {
    CURL *curl = curl_easy_init();
    if (!curl) return {0, "", "cannot initialize libcurl"};

    std::string response;
    char curl_error[CURL_ERROR_SIZE] = {0};
    struct curl_slist *headers = nullptr;
    struct curl_slist *resolve = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/soap+xml;charset=UTF-8");
    headers = curl_slist_append(headers, "Accept: application/soap+xml");
    headers = curl_slist_append(headers,
        "Authorization: http://schemas.dmtf.org/wbem/wsman/1/wsman/secprofile/https/mutual");
    std::string resolve_entry = hostname + ":" + std::to_string(config.winrm_port) + ":" + ip;
    resolve = curl_slist_append(resolve, resolve_entry.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, soap.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(soap.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve);
    curl_easy_setopt(curl, CURLOPT_CAINFO, config.ca_file.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLCERT, config.client_cert.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEY, config.client_key.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, config.verify_peer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, config.verify_host ? 2L : 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(config.connect_timeout_ms));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(config.request_timeout_ms));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_slist_free_all(resolve);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        return {status, response, curl_error[0] ? curl_error : curl_easy_strerror(result)};
    }
    return {status, response, ""};
}

std::string winrm_error(const WinrmHttpResult &result) {
    if (!result.error.empty()) return result.error;
    const std::string fault = xpath_text(result.body, "//*[local-name()='Message']");
    return "WinRM HTTP " + std::to_string(result.status) +
           (fault.empty() ? "" : ": " + fault);
}

std::string enumerate_soap(const std::string &url, const std::string &query,
                           unsigned int max_elements) {
    return
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        "xmlns:w=\"http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd\" "
        "xmlns:n=\"http://schemas.xmlsoap.org/ws/2004/09/enumeration\">"
        "<s:Header><a:To>" + xml_escape(url) + "</a:To>"
        "<w:ResourceURI s:mustUnderstand=\"true\">"
        "http://schemas.microsoft.com/wbem/wsman/1/wmi/root/cimv2/*"
        "</w:ResourceURI><a:ReplyTo><a:Address s:mustUnderstand=\"true\">"
        "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"
        "</a:Address></a:ReplyTo><a:Action s:mustUnderstand=\"true\">"
        "http://schemas.xmlsoap.org/ws/2004/09/enumeration/Enumerate</a:Action>"
        "<w:MaxEnvelopeSize s:mustUnderstand=\"true\">153600</w:MaxEnvelopeSize>"
        "<a:MessageID>uuid:" + message_id() + "</a:MessageID>"
        "<w:Locale xml:lang=\"en-US\" s:mustUnderstand=\"false\"/>"
        "<w:OperationTimeout>PT5S</w:OperationTimeout></s:Header>"
        "<s:Body><n:Enumerate><w:OptimizeEnumeration/><w:MaxElements>" +
        std::to_string(max_elements) + "</w:MaxElements>"
        "<w:Filter Dialect=\"http://schemas.microsoft.com/wbem/wsman/1/WQL\">" +
        xml_escape(query) + "</w:Filter></n:Enumerate></s:Body></s:Envelope>";
}

bool valid_sid(const std::string &sid) {
    return sid.size() >= 5 && sid.rfind("S-", 0) == 0 &&
           std::all_of(sid.begin() + 2, sid.end(), [](unsigned char c) {
               return std::isdigit(c) || c == '-';
           });
}

std::string profile_username(const std::string &path) {
    std::string value = trim(path);
    while (!value.empty() && (value.back() == '\\' || value.back() == '/')) value.pop_back();
    const std::size_t separator = value.find_last_of("\\/");
    if (separator != std::string::npos) value.erase(0, separator + 1);
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7f;
    }), value.end());
    if (value.empty() || value == "." || value == ".." || value.size() > 128 ||
        value.find('\\') != std::string::npos || value.find('/') != std::string::npos) {
        return "";
    }
    return value;
}

WinrmResult query_winrm(const Config &config, const std::string &ip,
                        const std::string &hostname) {
    if (!valid_hostname(hostname)) return {"", "invalid enrolled hostname"};
    const std::string url = "https://" + hostname + ":" +
                            std::to_string(config.winrm_port) + "/wsman";
    const std::string soap =
        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        "xmlns:w=\"http://schemas.dmtf.org/wbem/wsman/1/wsman.xsd\">"
        "<s:Header><a:To>" + xml_escape(url) + "</a:To>"
        "<w:ResourceURI s:mustUnderstand=\"true\">" + std::string(kResourceUri) +
        "</w:ResourceURI><a:ReplyTo><a:Address s:mustUnderstand=\"true\">"
        "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"
        "</a:Address></a:ReplyTo><a:Action s:mustUnderstand=\"true\">"
        "http://schemas.xmlsoap.org/ws/2004/09/transfer/Get</a:Action>"
        "<w:MaxEnvelopeSize s:mustUnderstand=\"true\">153600</w:MaxEnvelopeSize>"
        "<a:MessageID>uuid:" + message_id() + "</a:MessageID>"
        "<w:Locale xml:lang=\"en-US\" s:mustUnderstand=\"false\"/>"
        "<w:SelectorSet><w:Selector Name=\"Name\">" +
        xml_escape(hostname.substr(0, hostname.find('.'))) +
        "</w:Selector></w:SelectorSet><w:OperationTimeout>PT5S</w:OperationTimeout>"
        "</s:Header><s:Body/></s:Envelope>";

    const WinrmHttpResult system = winrm_post(config, ip, hostname, url, soap);
    std::string system_error;
    std::string username;
    if (!system.error.empty() || system.status != 200) {
        system_error = winrm_error(system);
    } else {
        username = xpath_text(system.body, "//*[local-name()='UserName']");
    }
    if (username.empty()) {
        const std::string profile_query =
            "SELECT LocalPath,SID,Loaded,Special FROM Win32_UserProfile "
            "WHERE Loaded=True AND Special=False";
        const WinrmHttpResult profiles = winrm_post(
            config, ip, hostname, url, enumerate_soap(url, profile_query, 32));
        if (!profiles.error.empty() || profiles.status != 200) {
            const std::string fallback_error = winrm_error(profiles);
            return {"", system_error.empty() ? fallback_error :
                system_error + "; session fallback: " + fallback_error};
        }

        const std::string sid = xpath_text(profiles.body, "//*[local-name()='SID']");
        const std::string local_path = xpath_text(profiles.body, "//*[local-name()='LocalPath']");
        std::string name;
        std::string domain;
        std::string account_error;
        if (valid_sid(sid)) {
            const std::string account_query =
                "SELECT Name,Domain,SID FROM Win32_UserAccount WHERE SID='" + sid + "'";
            const WinrmHttpResult account = winrm_post(
                config, ip, hostname, url, enumerate_soap(url, account_query, 8));
            if (account.error.empty() && account.status == 200) {
                name = xpath_text(account.body, "//*[local-name()='Name']");
                domain = xpath_text(account.body, "//*[local-name()='Domain']");
            } else {
                account_error = winrm_error(account);
            }

            /* Win32_UserAccount may return an empty result for a loaded domain
             * profile while the SID provider can still resolve it. */
            if (name.empty()) {
                const std::string sid_query =
                    "SELECT AccountName,ReferencedDomainName,SID FROM Win32_SID WHERE SID='" + sid + "'";
                const WinrmHttpResult sid_account = winrm_post(
                    config, ip, hostname, url, enumerate_soap(url, sid_query, 8));
                if (sid_account.error.empty() && sid_account.status == 200) {
                    name = xpath_text(sid_account.body, "//*[local-name()='AccountName']");
                    domain = xpath_text(sid_account.body, "//*[local-name()='ReferencedDomainName']");
                } else if (account_error.empty()) {
                    account_error = winrm_error(sid_account);
                }
            }
        }

        /* A least-privilege certificate-mapped WinRM account can read loaded
         * profiles while Windows deliberately hides the owning account and
         * RDP logon association.  The profile leaf is still the Windows user
         * name and is also one of the identity variants generated by AD Policy. */
        if (name.empty()) {
            name = profile_username(local_path);
            domain.clear();
        }
        if (name.empty()) {
            const bool no_interactive_user = account_error.empty();
            return {"", no_interactive_user ?
                "Windows reported no interactive user" : account_error,
                no_interactive_user};
        }
        username = domain.empty() ? name : domain + "\\" + name;
    }

    username.erase(std::remove_if(username.begin(), username.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7f;
    }), username.end());
    if (username.empty()) return {"", "Windows reported no interactive user", true};
    if (username.size() > 255) return {"", "Windows username exceeds limit"};
    return {username, ""};
}

std::string protocol_escape(const std::string &value) {
    std::ostringstream escaped;
    escaped << std::uppercase << std::hex;
    for (const unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '@') {
            escaped << static_cast<char>(c);
        } else {
            escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    return escaped.str();
}

std::string safe_message(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7f;
    }), value.end());
    if (value.size() > 160) value.resize(160);
    return protocol_escape(value);
}

struct Request {
    std::string channel;
    std::string ip;
};

Request parse_request(const std::string &line) {
    std::istringstream input(line);
    std::vector<std::string> fields;
    std::string field;
    while (input >> field) fields.push_back(field);
    if (fields.size() >= 2 && std::all_of(fields[0].begin(), fields[0].end(), ::isdigit)) {
        return {fields[0], fields[1]};
    }
    return {"", fields.empty() ? "" : fields[0]};
}

void reply(const Request &request, const std::string &username, const std::string &tag,
           const std::string &message = "") {
    if (!request.channel.empty()) std::cout << request.channel << ' ';
    std::cout << "OK user=" << protocol_escape(username) << " tag=" << protocol_escape(tag);
    if (!message.empty()) std::cout << " message=" << safe_message(message);
    std::cout << std::endl;
}

Identity unknown_identity(const Config &config, long long now) {
    return {config.unknown_user, "unknown", now + config.negative_ttl,
            now + config.negative_ttl};
}

int run_lookup(const Config &config, Database &database, const Request &request) {
    long long now = now_seconds();
    if (!valid_source_address(request.ip)) {
        reply(request, config.unknown_user, "invalid-source", "Invalid source address");
        return 1;
    }
    if (!config.enabled) {
        reply(request, config.unknown_user, "disabled", "Kontrol-ID is disabled");
        return 0;
    }

    auto cached = database.cached(request.ip);
    if (cached && cached->username != config.unknown_user && cached->expires_at > now) {
        reply(request, cached->username, cached->source);
        return 0;
    }

    const auto endpoint = database.endpoint(request.ip);
    if (!endpoint) {
        if (const auto portal = database.portal(request.ip, now)) {
            reply(request, portal->username, portal->source);
            return 0;
        }
        const auto unknown = unknown_identity(config, now);
        database.store(request.ip, unknown, "unknown", "Endpoint is not enrolled");
        reply(request, unknown.username, "needs-login", "Endpoint is not enrolled");
        return 0;
    }

    EndpointLookupLock lookup_lock(config.database, request.ip);
    now = now_seconds();
    cached = database.cached(request.ip);
    if (cached && cached->username != config.unknown_user && cached->expires_at > now) {
        reply(request, cached->username, cached->source);
        return 0;
    }
    if (!lookup_lock.acquired()) {
        /* Never let a burst from one slow or offline workstation consume the
         * complete helper pool.  The lock owner performs the network query;
         * concurrent requests immediately use the last verified user, or the
         * enrolled hostname only while that first lookup is still pending. */
        if (cached && cached->username != config.unknown_user &&
            cached->source == "winrm" && cached->stale_until > now) {
            reply(request, cached->username, "stale-winrm", "Identity refresh in progress");
            return 0;
        }
        if (valid_hostname(endpoint->hostname)) {
            std::string hostname = endpoint->hostname;
            std::transform(hostname.begin(), hostname.end(), hostname.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            reply(request, hostname, "hostname-pending", "Identity lookup in progress");
            return 0;
        }
        reply(request, config.unknown_user, "lookup-pending", "Identity lookup in progress");
        return 0;
    }

    const WinrmResult result = query_winrm(config, request.ip, endpoint->connection_name);
    if (!result.username.empty()) {
        Identity identity{result.username, "winrm", now + config.positive_ttl,
                          now + config.positive_ttl + config.stale_ttl};
        database.store(request.ip, identity, "identified", "");
        database.touch_endpoint(request.ip, now);
        reply(request, identity.username, identity.source);
        return 0;
    }

    if (!result.no_interactive_user &&
        cached && cached->username != config.unknown_user &&
        cached->source == "winrm" && cached->stale_until > now) {
        reply(request, cached->username, "stale-winrm", result.error);
        return 0;
    }

    /* Enrollment authenticates the workstation and records its hostname.  A
     * temporary WinRM/WMI failure must therefore degrade to that trusted
     * machine identity instead of treating an enrolled endpoint as unknown.
     * The unknown-user policy is reserved for endpoints for which neither a
     * logged-on user nor an enrolled hostname can be established. */
    if (valid_hostname(endpoint->hostname)) {
        std::string hostname = endpoint->hostname;
        std::transform(hostname.begin(), hostname.end(), hostname.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        /* A hostname is a safe machine identity, but it must not suppress a
         * new user lookup for the full positive-user TTL after a transient
         * WinRM failure. */
        Identity identity{hostname, "hostname", now + config.negative_ttl,
                          now + config.negative_ttl};
        database.store(request.ip, identity, "identified-hostname", result.error);
        database.touch_endpoint(request.ip, now);
        if (config.debug) {
            syslog(LOG_NOTICE, "Kontrol-ID: %s: using enrolled hostname %s after WinRM failure: %s",
                   request.ip.c_str(), hostname.c_str(), result.error.c_str());
        }
        reply(request, identity.username, identity.source);
        return 0;
    }

    if (const auto portal = database.portal(request.ip, now)) {
        reply(request, portal->username, portal->source);
        return 0;
    }

    const auto unknown = unknown_identity(config, now);
    database.store(request.ip, unknown, "unknown", result.error);
    if (config.debug) syslog(LOG_NOTICE, "Kontrol-ID: %s: %s", request.ip.c_str(), result.error.c_str());
    reply(request, unknown.username, "needs-login", result.error);
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    std::string config_path = "/usr/local/etc/kontrol-identity.conf";
    std::optional<std::string> one_shot;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if ((argument == "-c" || argument == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (argument == "--lookup" && i + 1 < argc) {
            one_shot = argv[++i];
        } else if (argument == "--version") {
            std::cout << "kontrol-identity-helper " << kVersion << std::endl;
            return 0;
        } else if (argument == "--help") {
            std::cout << "usage: kontrol-identity-helper [-c file] [--lookup address]" << std::endl;
            return 0;
        }
    }

    openlog("kontrol-identity-helper", LOG_PID, LOG_AUTHPRIV);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    xmlInitParser();
    try {
        const Config config = Config::load(config_path);
        Database database(config.database);
        if (one_shot) {
            const int status = run_lookup(config, database, Request{"", *one_shot});
            curl_global_cleanup();
            xmlCleanupParser();
            closelog();
            return status;
        }

        std::string line;
        while (std::getline(std::cin, line)) {
            const Request request = parse_request(line);
            try {
                run_lookup(config, database, request);
            } catch (const std::exception &error) {
                syslog(LOG_ERR, "Kontrol-ID request failed: %s", error.what());
                reply(request, config.unknown_user, "internal-error", error.what());
            }
        }
    } catch (const std::exception &error) {
        syslog(LOG_ERR, "Kontrol-ID helper startup failed: %s", error.what());
        std::cerr << "Kontrol-ID: " << error.what() << std::endl;
        curl_global_cleanup();
        xmlCleanupParser();
        closelog();
        return 1;
    }

    curl_global_cleanup();
    xmlCleanupParser();
    closelog();
    return 0;
}
