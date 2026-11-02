--- src/FatController.cpp.orig
+++ src/FatController.cpp
@@ -758,4 +758,5 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
     std::ofstream *logfile = NULL;
+    std::streamoff log_bytes = 0;
     if (!logsyslog) {
         logfile = new std::ofstream(log_location.c_str(), std::ios::app);
         if (logfile->fail()) {
@@ -773,2 +774,6 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
         }
     }
+    if (is_RQlog && o.rqlog_only_tunnel_open && logfile) {
+        std::streampos current_position = logfile->tellp();
+        log_bytes = current_position > 0 ? static_cast<std::streamoff>(current_position) : 0;
+    }
@@ -1067,8 +1072,8 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
             stringgroup = "0";
             groupname = is_RQlog ? "" : "none";
         } else {
             stringgroup = String(filtergroup + 1);
-            if (is_RQlog) {
+            if (is_RQlog && !o.rqlog_only_tunnel_open) {
                 groupname = "";
             } else {
                 if (stringcode == "407") {
@@ -1189,7 +1194,21 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
-        if (!logsyslog)
+        if (!logsyslog) {
+            if (is_RQlog && o.rqlog_only_tunnel_open && o.rqlog_max_bytes > 0 &&
+                log_bytes + static_cast<std::streamoff>(builtline.size() + 1) > o.rqlog_max_bytes) {
+                logfile->close();
+                logfile->open(log_location.c_str(), std::ios::out | std::ios::trunc);
+                if (logfile->fail()) {
+                    syslog(LOG_ERR, "%sError truncating bounded Real Time log file.", thread_id.c_str());
+                    return;
+                }
+                log_bytes = 0;
+            }
             *logfile << builtline << std::endl; // append the line
-        else
+            if (is_RQlog && o.rqlog_only_tunnel_open) {
+                log_bytes += static_cast<std::streamoff>(builtline.size() + 1);
+            }
+        } else {
             syslog(LOG_INFO, "%s", builtline.c_str());
+        }
 #ifdef E2DEBUG
         std::cerr << itemcount << " " << builtline << std::endl;
 #endif
