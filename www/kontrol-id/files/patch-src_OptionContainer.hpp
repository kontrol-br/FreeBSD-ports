--- src/OptionContainer.hpp.orig
+++ src/OptionContainer.hpp
@@ -142,6 +142,8 @@ public:
     std::string log_location;
     std::string RQlog_location;
     bool log_requests = false;
+    bool rqlog_only_tunnel_open = false;
+    int rqlog_max_bytes = 8388608;
     std::string pid_filename;
     std::string blocked_content_store;
     std::string monitor_helper;
