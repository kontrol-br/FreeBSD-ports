--- src/ConnectionHandler.cpp.orig
+++ src/ConnectionHandler.cpp
@@ -4739,6 +4739,10 @@
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
         doLog(clientuser, clientip, checkme);
     }
