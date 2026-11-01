--- src/ConnectionHandler.cpp.orig
+++ src/ConnectionHandler.cpp
@@ -896,0 +897 @@ int ConnectionHandler::handleConnection(Socket &peerconn, String &ip, bool ismit
+            bool protocolUpgrade = false;
@@ -1437,0 +1439,10 @@ int ConnectionHandler::handleConnection(Socket &peerconn, String &ip, bool ismit
+                        protocolUpgrade = header.isProtocolUpgrade()
+                                          && docheader.isProtocolUpgrade()
+                                          && rcode == 101;
+                        if (protocolUpgrade) {
+                            // The HTTP exchange ends at 101.  The remaining
+                            // bytes belong to the upgraded bidirectional
+                            // protocol and the connection cannot be reused.
+                            persistProxy = false;
+                            persistPeer = false;
+                        }
@@ -1461 +1472 @@ int ConnectionHandler::handleConnection(Socket &peerconn, String &ip, bool ismit
-                        if (docheader.contentLength() == 0)   // no content
+                        if (!protocolUpgrade && docheader.contentLength() == 0)   // no content
@@ -1488,0 +1500,7 @@ int ConnectionHandler::handleConnection(Socket &peerconn, String &ip, bool ismit
+            // Apply this after exception handling because an HTTP Upgrade is
+            // neither a response body nor a reusable persistent connection.
+            if (protocolUpgrade) {
+                checkme.tunnel_rest = false;
+                checkme.tunnel_2way = true;
+            }
+
@@ -1510 +1528 @@ int ConnectionHandler::handleConnection(Socket &peerconn, String &ip, bool ismit
-            if (fg_resp && (!checkme.isItNaughty) && (!checkme.upfailure) && (!checkme.isconnect) && (!checkme.logcategory) && !checkme.tunnel_rest) {
+            if (fg_resp && (!checkme.isItNaughty) && (!checkme.upfailure) && (!checkme.isconnect) && (!checkme.logcategory) && !checkme.tunnel_rest && !protocolUpgrade) {
@@ -3021,2 +3039,2 @@ bool ConnectionHandler::goMITM(NaughtyFilter &checkme, Socket &proxysock, Socket
-            writecert = o.ca->writeCertificate(checkme.urldomain.c_str(), cert,
-                                               &caser);
+            writecert = o.ca->writeCertificate(checkme.urldomain.c_str(), cert,
+                                               &caser, checkme.isiphost);
@@ -4739,4 +4757,8 @@ int ConnectionHandler::handleICAPresmod(Socket &peerconn, String &ip, NaughtyFil
-            //Log
-    if (!checkme.isourwebserver && checkme.isItNaughty) { // don't log requests to the web server & and normal response
-        checkme.whatIsNaughtyLog = "ICAP Response filtering: ";
-        checkme.whatIsNaughtyLog += checkme.whatIsNaughty;
+    // REQMOD deliberately defers clean-request logging to RESPMOD so the
+    // final status, size and elapsed time are available.  Log every normal
+    // response here; blocked requests have already been logged by REQMOD.
+    if (!checkme.isourwebserver) {
+        if (checkme.isItNaughty) {
+            checkme.whatIsNaughtyLog = "ICAP Response filtering: ";
+            checkme.whatIsNaughtyLog += checkme.whatIsNaughty;
+        }
