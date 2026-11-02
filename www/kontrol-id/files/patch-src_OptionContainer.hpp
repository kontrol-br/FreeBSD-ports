--- src/OptionContainer.hpp.orig
+++ src/OptionContainer.hpp
@@ -142,6 +142,7 @@ public:
     std::string log_location;
     std::string RQlog_location;
     bool log_requests = false;
+    bool rqlog_only_tunnel_open = false;
     std::string pid_filename;
     std::string blocked_content_store;
     std::string monitor_helper;
