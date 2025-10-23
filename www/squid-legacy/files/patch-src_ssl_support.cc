--- src/ssl/support.cc.orig	2021-05-30 20:00:00 UTC
+++ src/ssl/support.cc
@@
-    ssl_ex_index_cert_error_check = SSL_get_ex_new_index(0, (void *) "cert_error_check", NULL, &ssl_dupAclChecklist, &ssl_freeAclChecklist);
+#if OPENSSL_VERSION_NUMBER >= 0x30000000L
+    ssl_ex_index_cert_error_check = SSL_get_ex_new_index(0, (void *) "cert_error_check", NULL, &ssl_dupAclChecklist_ossl3, &ssl_freeAclChecklist);
+#else
+    ssl_ex_index_cert_error_check = SSL_get_ex_new_index(0, (void *) "cert_error_check", NULL, &ssl_dupAclChecklist, &ssl_freeAclChecklist);
+#endif
@@
     CRYPTO_set_ex_data(to, idx, dupChecklist);
     return 1;
 }

+#if OPENSSL_VERSION_NUMBER >= 0x30000000L
+static int
+ssl_dupAclChecklist_ossl3(CRYPTO_EX_DATA *to, const CRYPTO_EX_DATA *from,
+    void **from_d, int idx, long argl, void *argp)
+{
+    void *data = from_d ? *from_d : NULL;
+    return ssl_dupAclChecklist(to, const_cast<CRYPTO_EX_DATA *>(from), data, idx, argl, argp);
+}
+#endif
