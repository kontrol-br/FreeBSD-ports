--- src/FatController.cpp.orig
+++ src/FatController.cpp
@@ -1068,7 +1068,7 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
             stringgroup = "0";
             groupname = is_RQlog ? "" : "none";
         } else {
             stringgroup = String(filtergroup + 1);
-            if (is_RQlog) {
+            if (is_RQlog && !o.rqlog_only_tunnel_open) {
                 groupname = "";
             } else {
                 if (stringcode == "407") {
