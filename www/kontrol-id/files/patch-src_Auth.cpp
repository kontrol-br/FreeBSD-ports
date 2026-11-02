--- src/Auth.cpp.orig	2026-10-31 22:00:00 UTC
+++ src/Auth.cpp
@@ -19,4 +19,10 @@
 #include <algorithm>
 #include <cctype>
+#include <atomic>
+#include <fstream>
+#include <memory>
+#include <mutex>
 #include <sstream>
+#include <unordered_set>
 #include <arpa/inet.h>
+#include <time.h>
@@ -36,1 +41,5 @@
 extern authcreate_t PF_basic_create;
+extern authcreate_t kontrol_negotiate_create;
+extern authcreate_t kontrol_ntlm_create;
+extern authcreate_t kontrol_basic_create;
+extern authcreate_t kontrol_identity_create;
@@ -73,1 +83,91 @@
 }
+
+struct kontrol_user_policy_snapshot
+{
+    std::unordered_set<std::string> banned;
+    std::unordered_set<std::string> exception;
+};
+
+std::mutex kontrol_user_policy_lock;
+std::shared_ptr<const kontrol_user_policy_snapshot> kontrol_user_policy_current;
+std::atomic<time_t> kontrol_user_policy_refreshed(0);
+
+std::string lowercase_copy(std::string value)
+{
+    std::transform(value.begin(), value.end(), value.begin(),
+        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
+    return value;
+}
+
+void load_kontrol_user_list(const char *path,
+    std::unordered_set<std::string> &target)
+{
+    std::ifstream input(path);
+    std::string line;
+    while (std::getline(input, line)) {
+        const std::string::size_type comment = line.find('#');
+        if (comment != std::string::npos)
+            line.erase(comment);
+        std::istringstream fields(line);
+        std::string identity;
+        fields >> identity;
+        identity = lowercase_copy(normalise_auth_username(identity));
+        if (!identity.empty() && identity != "-")
+            target.insert(identity);
+    }
+}
+
+std::shared_ptr<const kontrol_user_policy_snapshot> kontrol_user_policy()
+{
+    const time_t now = time(NULL);
+    std::shared_ptr<const kontrol_user_policy_snapshot> current =
+        std::atomic_load(&kontrol_user_policy_current);
+    if (current && kontrol_user_policy_refreshed.load() == now)
+        return current;
+
+    std::lock_guard<std::mutex> guard(kontrol_user_policy_lock);
+    current = std::atomic_load(&kontrol_user_policy_current);
+    if (!current || kontrol_user_policy_refreshed.load() != now) {
+        std::shared_ptr<kontrol_user_policy_snapshot> next(
+            new kontrol_user_policy_snapshot());
+        load_kontrol_user_list(
+            "/usr/local/etc/e2guardian/lists/authplugins/exceptionuserlist",
+            next->exception);
+        load_kontrol_user_list(
+            "/usr/local/etc/e2guardian/lists/authplugins/banneduserlist",
+            next->banned);
+        std::atomic_store(&kontrol_user_policy_current,
+            std::shared_ptr<const kontrol_user_policy_snapshot>(next));
+        kontrol_user_policy_refreshed.store(now);
+    }
+    return std::atomic_load(&kontrol_user_policy_current);
+}
+
+int apply_kontrol_user_policy(const std::string &user, NaughtyFilter &check)
+{
+    const std::shared_ptr<const kontrol_user_policy_snapshot> policy =
+        kontrol_user_policy();
+    if (!policy)
+        return 0;
+    const std::string identity = lowercase_copy(normalise_auth_username(user));
+    if (identity.empty())
+        return 0;
+    if (policy->banned.find(identity) != policy->banned.end()) {
+        check.isBlocked = true;
+        check.isItNaughty = true;
+        check.blocktype = 0;
+        check.whatIsNaughty = "Access denied by the Kontrol-ID user policy.";
+        check.whatIsNaughtyLog = "User denied by Kontrol-ID banned user list";
+        check.whatIsNaughtyCategories = "User policy";
+        check.whatIsNaughtyDisplayCategories = "User policy";
+        return 1;
+    }
+    if (policy->exception.find(identity) != policy->exception.end()) {
+        check.isexception = true;
+        check.isException = true;
+        check.nomitm = true;
+        check.automitm = false;
+        return 2;
+    }
+    return 0;
+}
@@ -163,2 +259,18 @@
     user = u.toCharArray(); // also pass back to ConnectionHandler, so appears lowercase in logs
+    const int user_policy = apply_kontrol_user_policy(user, cm);
+    if (user_policy != 0) {
+        int selected = get_default(!cm.request_header->isProxyRequest);
+        fg = selected > 0 ? selected - 1 : 0;
+        cm.filtergroup = fg;
+        cm.user = user;
+        if (cm.authrec != nullptr) {
+            cm.authrec->filter_group = fg;
+            cm.authrec->group_source = user_policy == 1 ?
+                "banned-user" : "exception-user";
+            if (cm.authrec->user_name.length() == 0)
+                cm.authrec->user_name = user;
+            cm.authrec->is_authed = true;
+        }
+        return E2AUTH_OK;
+    }
     std::string entry_function_name;
@@ -272,1 +384,17 @@
+    if (plugname == "kontrol-basic") {
+        return kontrol_basic_create(cv);
+    }
+
+    if (plugname == "kontrol-negotiate") {
+        return kontrol_negotiate_create(cv);
+    }
+
+    if (plugname == "kontrol-ntlm") {
+        return kontrol_ntlm_create(cv);
+    }
+
+    if (plugname == "kontrol-identity") {
+        return kontrol_identity_create(cv);
+    }
+
     if (plugname == "pf-basic") {
