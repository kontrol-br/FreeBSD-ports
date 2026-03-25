--- src/HttpHeaderTools.h.orig	2021-08-23 03:27:17 UTC
+++ src/HttpHeaderTools.h
@@ -67,7 +67,7 @@
 private:
     /// Case-insensitive std::string "less than" comparison functor.
     /// Fast version recommended by Meyers' "Effective STL" for ASCII c-strings.
-    class NoCaseLessThan: public std::binary_function<std::string, std::string, bool>
+    class NoCaseLessThan
     {
     public:
         bool operator()(const std::string &lhs, const std::string &rhs) const {
