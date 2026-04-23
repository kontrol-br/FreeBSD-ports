--- server/drivers/hd44780-low.h.orig	2026-04-23 00:00:00 UTC
+++ server/drivers/hd44780-low.h
@@ -15,7 +15,11 @@
 #endif
 
 #ifdef HAVE_LIBUSB_1_0
-# include <libusb-1.0/libusb.h>
+# ifdef __FreeBSD__
+#  include <libusb.h>
+# else
+#  include <libusb-1.0/libusb.h>
+# endif
 #endif
 
 #if TIME_WITH_SYS_TIME
