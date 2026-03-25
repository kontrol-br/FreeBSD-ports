--- src/HttpHeaderTools.h.orig	2021-08-23 03:27:17 UTC
+++ src/HttpHeaderTools.h
@@ -67,7 +67,7 @@
 class HttpHeaderFieldAttrs;
 
 class HttpHeaderFieldStat {
-    class NoCaseLessThan: public std::binary_function<std::string, std::string, bool>
+    class NoCaseLessThan
     {
     public:
         bool operator() (const std::string &lhs, const std::string &rhs) const
