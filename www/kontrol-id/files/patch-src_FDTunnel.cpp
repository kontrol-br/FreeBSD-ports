--- src/FDTunnel.cpp.orig
+++ src/FDTunnel.cpp
@@ -129,11 +129,10 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-#ifdef __SSLMITM
-        // TODO: Post v5.3 change socket logic to non-blocking so that poll can be used in MITM
-        // after read/write - PP
-     if (sockfrom.isSsl()) {
-      twayfds[0].revents = POLLIN;
-      twayfds[1].revents = 0;
-  } else
-#endif
-        {
-            int rc = poll(twayfds, 2, timeout);
-            if (rc < 1) {
+        // poll the descriptors in both directions even under TLS.  OpenSSL can
+        // also retain decrypted bytes after the descriptor stops being
+        // readable, so mark those sockets ready before entering poll().
+        bool frombuffered = sockfrom.hasBufferedInput();
+        bool tobuffered = twoway && sockto.hasBufferedInput();
+        twayfds[0].revents = frombuffered ? POLLIN : 0;
+        twayfds[1].revents = tobuffered ? POLLIN : 0;
+        int poll_timeout = (frombuffered || tobuffered) ? 0 : timeout;
+        int pollrc = poll(twayfds, 2, poll_timeout);
+        if (pollrc < 0 || (pollrc == 0 && !frombuffered && !tobuffered)) {
@@ -141,2 +140,2 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-                std::cout <<thread_id << "tunnel tw poll returned error or timeout::" << rc
-                  << std::endl;
+            std::cout <<thread_id << "tunnel tw poll returned error or timeout::" << pollrc
+                      << std::endl;
@@ -144,2 +143,6 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-                break; // an error occurred or it timed out so end while()
-            }
+            break; // an error occurred or it timed out so end while()
+        }
+        if (frombuffered)
+            twayfds[0].revents |= POLLIN;
+        if (tobuffered)
+            twayfds[1].revents |= POLLIN;
@@ -147 +150 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-            std::cout <<thread_id << "tunnel tw poll returned ok:" << rc
+        std::cout <<thread_id << "tunnel tw poll returned ok:" << pollrc
@@ -150 +152,0 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-                  }
@@ -163 +165 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-                    rc = sockfrom.readFromSocket(buff.data(), readlen, 0, 0, false);
+                    rc = sockfrom.readFromSocketOnce(buff.data(), readlen, 0, 0);
@@ -165 +167 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-                    rc = sockfrom.readFromSocket(buff.data(), static_cast<int>(buff.size()), 0, 0, false);
+                    rc = sockfrom.readFromSocketOnce(buff.data(), static_cast<int>(buff.size()), 0, 0);
@@ -169 +171,3 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-                if (rc < 0) {
+                if (rc == -2) {
+                    done = false; // OpenSSL wants another readiness event
+                } else if (rc < 0) {
@@ -215 +219 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-                rc = sockto.readFromSocket(buff.data(), static_cast<int>(buff.size()), 0, 0, false);
+                rc = sockto.readFromSocketOnce(buff.data(), static_cast<int>(buff.size()), 0, 0);
@@ -217 +221,3 @@ bool FDTunnel::tunnel(Socket &sockfrom, Socket &sockto, bool twoway, off_t targe
-                if (rc < 0) {
+                if (rc == -2) {
+                    done = false; // OpenSSL wants another readiness event
+                } else if (rc < 0) {
