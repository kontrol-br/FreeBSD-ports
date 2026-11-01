--- src/Socket.hpp.orig
+++ src/Socket.hpp
@@ -58,0 +59,7 @@ class Socket : public BaseSocket
+    // Streaming protocols need one non-greedy read per readiness event.
+    // readFromSocket() intentionally fills the requested buffer on TLS, which
+    // is correct for HTTP bodies but would stall small WebSocket frames.
+    bool hasBufferedInput();
+    int readFromSocketOnce(char *buff, int len, unsigned int flags,
+                           int timeout);
+
