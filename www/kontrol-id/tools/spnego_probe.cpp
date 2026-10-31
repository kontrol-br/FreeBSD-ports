// Build-only diagnostic used to exercise Kontrol-ID's proxy Negotiate path.
// It emits one base64-encoded SPNEGO initiator token for an HTTP service.

#include <gssapi/gssapi.h>

#include <cstdio>
#include <cstdlib>
#include <string>

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

void print_status(const char *label, OM_uint32 status, int type)
{
    OM_uint32 minor = 0;
    OM_uint32 context = 0;
    gss_buffer_desc text = GSS_C_EMPTY_BUFFER;
    do {
        gss_display_status(&minor, status, type, GSS_C_NO_OID,
            &context, &text);
        std::fprintf(stderr, "%s: %.*s\n", label,
            static_cast<int>(text.length), static_cast<char *>(text.value));
        gss_release_buffer(&minor, &text);
    } while (context != 0);
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s proxy-fqdn\n", argv[0]);
        return 64;
    }

    const std::string service = std::string("HTTP@") + argv[1];
    gss_buffer_desc service_buffer;
    service_buffer.value = const_cast<char *>(service.data());
    service_buffer.length = service.size();

    OM_uint32 major = 0;
    OM_uint32 minor = 0;
    gss_name_t target = GSS_C_NO_NAME;
    major = gss_import_name(&minor, &service_buffer,
        GSS_C_NT_HOSTBASED_SERVICE, &target);
    if (major != GSS_S_COMPLETE) {
        print_status("gss_import_name", major, GSS_C_GSS_CODE);
        return 1;
    }

    // SPNEGO mechanism OID: 1.3.6.1.5.5.2
    static unsigned char spnego_bytes[] = {0x2b, 0x06, 0x01, 0x05, 0x05, 0x02};
    static gss_OID_desc spnego = {sizeof(spnego_bytes), spnego_bytes};
    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    gss_buffer_desc output = GSS_C_EMPTY_BUFFER;
    major = gss_init_sec_context(&minor, GSS_C_NO_CREDENTIAL, &context,
        target, &spnego, GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG,
        0, GSS_C_NO_CHANNEL_BINDINGS, GSS_C_NO_BUFFER, NULL, &output,
        NULL, NULL);
    gss_release_name(&minor, &target);
    if (major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED) {
        print_status("gss_init_sec_context", major, GSS_C_GSS_CODE);
        print_status("gss_init_sec_context minor", minor, GSS_C_MECH_CODE);
        return 1;
    }

    const std::string encoded = base64_encode(output.value, output.length);
    gss_release_buffer(&minor, &output);
    if (context != GSS_C_NO_CONTEXT)
        gss_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
    if (encoded.empty())
        return 1;
    std::puts(encoded.c_str());
    return 0;
}
