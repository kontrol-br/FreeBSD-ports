// Kontrol-ID native Kerberos/SPNEGO authentication plugin.
//
// Accepts Proxy-Authorization: Negotiate directly in E2Guardian's explicit
// proxy listener.  The entire GSS negotiation is completed inside identify(),
// so the security context is local to the client connection/thread.

#ifdef HAVE_CONFIG_H
#include "e2config.h"
#endif

#include "../Auth.hpp"
#include "../NaughtyFilter.hpp"
#include "../OptionContainer.hpp"
#include "../StoryBoard.hpp"

#include <gssapi/gssapi.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <syslog.h>

extern bool is_daemonised;
extern OptionContainer o;
extern thread_local std::string thread_id;

namespace {

std::string base64_encode(const void *data, size_t length)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *input = static_cast<const unsigned char *>(data);
    std::string result;
    result.reserve(((length + 2) / 3) * 4);

    for (size_t offset = 0; offset < length; offset += 3) {
        const unsigned int a = input[offset];
        const unsigned int b = offset + 1 < length ? input[offset + 1] : 0;
        const unsigned int c = offset + 2 < length ? input[offset + 2] : 0;
        const unsigned int value = (a << 16) | (b << 8) | c;
        result.push_back(alphabet[(value >> 18) & 0x3f]);
        result.push_back(alphabet[(value >> 12) & 0x3f]);
        result.push_back(offset + 1 < length ? alphabet[(value >> 6) & 0x3f] : '=');
        result.push_back(offset + 2 < length ? alphabet[value & 0x3f] : '=');
    }
    return result;
}

void log_gss_status(const char *operation, OM_uint32 major, OM_uint32 minor)
{
    OM_uint32 context = 0;
    OM_uint32 ignored = 0;
    gss_buffer_desc message = GSS_C_EMPTY_BUFFER;
    do {
        if (gss_display_status(&ignored, major, GSS_C_GSS_CODE,
                GSS_C_NO_OID, &context, &message) == GSS_S_COMPLETE) {
            syslog(LOG_ERR, "Kontrol-ID Negotiate %s: %.*s (minor=%lu)",
                operation, static_cast<int>(message.length),
                static_cast<const char *>(message.value),
                static_cast<unsigned long>(minor));
            gss_release_buffer(&ignored, &message);
        }
    } while (context != 0);
}

bool send_challenge(Socket &peercon, const gss_buffer_desc *token)
{
    std::string response =
        "HTTP/1.1 407 Proxy Authentication Required\r\n"
        "Proxy-Authenticate: Negotiate";
    if (token != NULL && token->length > 0) {
        response += " ";
        response += base64_encode(token->value, token->length);
    }
    response +=
        "\r\nProxy-Connection: keep-alive"
        "\r\nConnection: keep-alive"
        "\r\nContent-Length: 0"
        "\r\nCache-Control: no-store"
        "\r\n\r\n";
    return peercon.writeString(response);
}

bool is_negotiate(HTTPHeader &header)
{
    String type(header.getAuthType());
    type.toLower();
    return type == "negotiate";
}

} // namespace

class kontrol_negotiate_instance : public AuthPlugin
{
public:
    explicit kontrol_negotiate_instance(ConfigVar &definition)
        : AuthPlugin(definition), acceptor_cred(GSS_C_NO_CREDENTIAL)
    {
        is_connection_based = true;
        needs_proxy_query = false;
        needs_proxy_access_in_plugin = false;
        client_ip_based = false;
    }

    ~kontrol_negotiate_instance() override
    {
        quit();
    }

    int init(void *args) override;
    int quit() override;
    int identify(Socket &peercon, Socket &proxycon, HTTPHeader &header,
        std::string &username, bool &is_real_user, auth_rec &authrec) override;

private:
    gss_cred_id_t acceptor_cred;
};

AuthPlugin *kontrol_negotiate_create(ConfigVar &definition)
{
    return new kontrol_negotiate_instance(definition);
}

int kontrol_negotiate_instance::init(void *args)
{
    const std::string keytab = cv["keytab"].toCharArray();
    if (keytab.empty()) {
        syslog(LOG_ERR, "Kontrol-ID Negotiate: keytab is not configured");
        return -1;
    }
    if (setenv("KRB5_KTNAME", keytab.c_str(), 1) != 0) {
        syslog(LOG_ERR, "Kontrol-ID Negotiate: unable to set KRB5_KTNAME");
        return -1;
    }

    OM_uint32 major = 0;
    OM_uint32 minor = 0;
    gss_name_t service_name = GSS_C_NO_NAME;
    gss_buffer_desc service_buffer = GSS_C_EMPTY_BUFFER;
    const std::string principal = cv["service_principal"].toCharArray();
    if (!principal.empty()) {
        service_buffer.value = const_cast<char *>(principal.data());
        service_buffer.length = principal.size();
        major = gss_import_name(&minor, &service_buffer,
            GSS_C_NT_USER_NAME, &service_name);
        if (major != GSS_S_COMPLETE) {
            log_gss_status("gss_import_name", major, minor);
            return -1;
        }
    }

    major = gss_acquire_cred(&minor, service_name, GSS_C_INDEFINITE,
        GSS_C_NO_OID_SET, GSS_C_ACCEPT, &acceptor_cred, NULL, NULL);
    if (service_name != GSS_C_NO_NAME)
        gss_release_name(&minor, &service_name);
    if (major != GSS_S_COMPLETE) {
        log_gss_status("gss_acquire_cred", major, minor);
        acceptor_cred = GSS_C_NO_CREDENTIAL;
        return -1;
    }

    OptionContainer::auth_entry entry;
    entry.entry_function = cv["story_function"];
    if (entry.entry_function.length() < 1) {
        syslog(LOG_ERR, "Kontrol-ID Negotiate: story_function is not configured");
        quit();
        return -1;
    }
    entry.entry_id = ENT_STORYA_AUTH_PF_BASIC;
    story_entry = entry.entry_id;
    o.auth_entry_dq.push_back(entry);
    read_def_fg();
    return 0;
}

int kontrol_negotiate_instance::quit()
{
    if (acceptor_cred != GSS_C_NO_CREDENTIAL) {
        OM_uint32 minor = 0;
        gss_release_cred(&minor, &acceptor_cred);
        acceptor_cred = GSS_C_NO_CREDENTIAL;
    }
    return 0;
}

int kontrol_negotiate_instance::identify(Socket &peercon, Socket &proxycon,
    HTTPHeader &header, std::string &username, bool &is_real_user,
    auth_rec &authrec)
{
    (void)proxycon;
    if (!header.isProxyRequest)
        return E2AUTH_NOMATCH;

    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    gss_name_t client_name = GSS_C_NO_NAME;
    OM_uint32 major = GSS_S_CONTINUE_NEEDED;
    OM_uint32 minor = 0;
    unsigned int rounds = 0;

    while (major == GSS_S_CONTINUE_NEEDED && rounds++ < 5) {
        if (!is_negotiate(header)) {
            // Do not fall through to E2Guardian's unauthenticated default for
            // an unsupported proxy scheme.  This plugin is authoritative for
            // its listener and must keep challenging until Negotiate is used.
            if (!send_challenge(peercon, NULL) || !header.in(&peercon, true))
                return -1;
            continue;
        }

        const std::string token = header.getAuthData();
        if (token.empty()) {
            if (!send_challenge(peercon, NULL) || !header.in(&peercon, true))
                return -1;
            continue;
        }

        gss_buffer_desc input;
        input.value = const_cast<char *>(token.data());
        input.length = token.size();
        gss_buffer_desc output = GSS_C_EMPTY_BUFFER;
        major = gss_accept_sec_context(&minor, &context, acceptor_cred,
            &input, GSS_C_NO_CHANNEL_BINDINGS, &client_name, NULL, &output,
            NULL, NULL, NULL);

        if (major == GSS_S_CONTINUE_NEEDED) {
            const bool sent = send_challenge(peercon, &output);
            gss_release_buffer(&minor, &output);
            if (!sent || !header.in(&peercon, true)) {
                gss_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
                return -1;
            }
            continue;
        }

        if (output.length > 0)
            gss_release_buffer(&minor, &output);
        if (major != GSS_S_COMPLETE) {
            log_gss_status("gss_accept_sec_context", major, minor);
            send_challenge(peercon, NULL);
            if (context != GSS_C_NO_CONTEXT)
                gss_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
            if (client_name != GSS_C_NO_NAME)
                gss_release_name(&minor, &client_name);
            return -1;
        }
    }

    if (major != GSS_S_COMPLETE || client_name == GSS_C_NO_NAME) {
        if (context != GSS_C_NO_CONTEXT)
            gss_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
        return -1;
    }

    gss_buffer_desc display = GSS_C_EMPTY_BUFFER;
    major = gss_display_name(&minor, client_name, &display, NULL);
    if (major == GSS_S_COMPLETE && display.length > 0)
        username.assign(static_cast<const char *>(display.value), display.length);
    gss_release_buffer(&minor, &display);
    gss_release_name(&minor, &client_name);
    gss_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);

    if (major != GSS_S_COMPLETE || username.empty()) {
        log_gss_status("gss_display_name", major, minor);
        return -1;
    }

    username = normalise_auth_username(username);
    authrec.user_name = username;
    authrec.user_source = "kontrol-negotiate";
    authrec.is_authed = true;
    is_real_user = true;
    return E2AUTH_OK;
}
