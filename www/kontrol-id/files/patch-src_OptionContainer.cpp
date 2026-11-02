--- src/OptionContainer.cpp.orig
+++ src/OptionContainer.cpp
@@ -186,3 +186,4 @@ bool OptionContainer::read(const char *filename, int type) {
             } else {
                 log_requests = true;
             }
+            rqlog_only_tunnel_open = (findoptionS("rqlogonlytunnelopen") == "on");
