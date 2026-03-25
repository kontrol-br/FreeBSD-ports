--- src/acl/InnerNode.cc.orig	2021-08-23 03:27:17 UTC
+++ src/acl/InnerNode.cc
@@ -16,12 +16,13 @@
 #include "Debug.h"
 #include "globals.h"
 #include <algorithm>
+#include <functional>
 
 void
 Acl::InnerNode::prepareForUse()
 {
-    std::for_each(nodes.begin(), nodes.end(), std::mem_fun(&ACL::prepareForUse));
+    std::for_each(nodes.begin(), nodes.end(), std::mem_fn(&ACL::prepareForUse));
 }
 
 bool
 Acl::InnerNode::empty() const
