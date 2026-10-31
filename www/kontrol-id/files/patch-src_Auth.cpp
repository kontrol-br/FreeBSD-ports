--- src/Auth.cpp.orig	2026-10-31 22:00:00 UTC
+++ src/Auth.cpp
@@ -29,1 +29,4 @@
 extern authcreate_t PF_basic_create;
+extern authcreate_t kontrol_negotiate_create;
+extern authcreate_t kontrol_ntlm_create;
+extern authcreate_t kontrol_basic_create;
@@ -210,1 +213,13 @@
+    if (plugname == "kontrol-basic") {
+        return kontrol_basic_create(cv);
+    }
+
+    if (plugname == "kontrol-negotiate") {
+        return kontrol_negotiate_create(cv);
+    }
+
+    if (plugname == "kontrol-ntlm") {
+        return kontrol_ntlm_create(cv);
+    }
+
     if (plugname == "pf-basic") {
