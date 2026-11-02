--- src/OptionContainer.cpp.orig
+++ src/OptionContainer.cpp
@@ -186,3 +186,8 @@ bool OptionContainer::read(const char *filename, int type) {
             } else {
                 log_requests = true;
             }
+            rqlog_only_tunnel_open = (findoptionS("rqlogonlytunnelopen") == "on");
+            rqlog_max_bytes = findoptionI("rqlogmaxbytes");
+            if (rqlog_max_bytes < 1048576 || rqlog_max_bytes > 67108864) {
+                rqlog_max_bytes = 8388608;
+            }
