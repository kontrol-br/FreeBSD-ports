--- src/base/LookupTable.h.orig	2021-08-23 03:27:17 UTC
+++ src/base/LookupTable.h
@@ -47,7 +47,7 @@
 /// Implement algorithm expected by Splay
 /// using char* compare, to preserve historical behavior
 /// and minimize refactoring.
-class SBufCaseInsensitiveLess : public std::binary_function<SBuf, SBuf, bool> {
+class SBufCaseInsensitiveLess {
 public:
     bool operator() (const SBuf &, const SBuf &) const;
 };
