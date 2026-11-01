--- src/Socket.cpp.orig
+++ src/Socket.cpp
@@ -1172,0 +1173,56 @@ int Socket::readFromSocket(char *buff, int len, unsigned int flags, int timeout,
+bool Socket::hasBufferedInput()
+{
+    if ((bufflen - buffstart) > 0)
+        return true;
+#ifdef __SSLMITM
+    if (isssl)
+        return SSL_pending(ssl) > 0;
+#endif
+    return false;
+}
+
+int Socket::readFromSocketOnce(char *buff, int len, unsigned int flags, int timeout)
+{
+    if (len <= 0)
+        return 0;
+
+    // Header parsing can leave bytes in Socket's read-ahead buffer.  Deliver
+    // those before touching the descriptor or OpenSSL.
+    if ((bufflen - buffstart) > 0) {
+        int available = bufflen - buffstart;
+        int tocopy = available < len ? available : len;
+        memcpy(buff, buffer + buffstart, tocopy);
+        buffstart += tocopy;
+        if (buffstart == bufflen) {
+            buffstart = 0;
+            bufflen = 0;
+        }
+        return tocopy;
+    }
+
+#ifdef __SSLMITM
+    if (isssl) {
+        ERR_clear_error();
+        int rc = SSL_read(ssl, buff, len);
+        if (rc > 0)
+            return rc;
+
+        int ssl_error = SSL_get_error(ssl, rc);
+        if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
+            return -2;
+        if (ssl_error == SSL_ERROR_ZERO_RETURN) {
+            ishup = true;
+            return 0;
+        }
+
+        s_errno = errno;
+        log_ssl_errors("ssl streaming read failed %s", "");
+        return -1;
+    }
+#endif
+
+    // poll() in FDTunnel has already established readiness, so avoid a second
+    // blocking readiness check here.
+    return BaseSocket::readFromSocket(buff, len, flags, timeout, false, false);
+}
+
