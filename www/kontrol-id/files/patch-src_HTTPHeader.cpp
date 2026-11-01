--- src/HTTPHeader.cpp.orig
+++ src/HTTPHeader.cpp
@@ -49,0 +50 @@ void HTTPHeader::reset()
+        isprotocolupgrade = false;
@@ -78,0 +80 @@ void HTTPHeader::reset()
+        pupgrade = NULL;
@@ -984,0 +987,2 @@ void HTTPHeader::checkheader(bool allowpersistent)
+        } else if ((pupgrade == NULL) && i->startsWithLower("upgrade:")) {
+            pupgrade = &(*i);
@@ -1053,0 +1058,9 @@ void HTTPHeader::checkheader(bool allowpersistent)
+    // Upgrade is a hop-by-hop header, but it must survive this proxy hop for
+    // WebSocket and other HTTP/1.1 protocol upgrades.  Do not treat an
+    // isolated Upgrade header as sufficient: Connection must nominate it.
+    if (onepointone && pproxyconnection != NULL && pupgrade != NULL) {
+        String connection_value = *pproxyconnection;
+        connection_value.toLower();
+        isprotocolupgrade = connection_value.contains("upgrade");
+    }
+
@@ -1110,0 +1124,6 @@ if(!icap) {
+    // An upgraded connection is no longer an HTTP persistent connection.  Its
+    // original Connection/Upgrade fields are forwarded unchanged and the
+    // connection handler takes ownership after a matching 101 response.
+    if (isprotocolupgrade) {
+        allowpersistent = false;
+        waspersistent = false;
@@ -1113 +1132 @@ if(!icap) {
-    if (allowpersistent) {
+    } else if (allowpersistent) {
