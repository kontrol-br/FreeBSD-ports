--- src/HTTPHeader.hpp.orig
+++ src/HTTPHeader.hpp
@@ -133,0 +134,7 @@ class HTTPHeader
+    // HTTP/1.1 protocol upgrades (most notably WebSocket) must retain their
+    // hop-by-hop headers until the handler switches to a bidirectional tunnel.
+    bool isProtocolUpgrade() const
+    {
+        return isprotocolupgrade;
+    };
+
@@ -235,0 +243 @@ class HTTPHeader
+    String *pupgrade;
@@ -259,0 +268 @@ class HTTPHeader
+    bool isprotocolupgrade;
