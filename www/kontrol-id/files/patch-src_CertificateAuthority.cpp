--- src/CertificateAuthority.cpp.orig
+++ src/CertificateAuthority.cpp
@@ -15,6 +15,8 @@
 #include <climits>
+#include <cctype>
 #include <string>
 #include <iostream>
+#include <utility>
 
 #include <openssl/pem.h>
 #include <openssl/evp.h>
@@ -110,15 +112,98 @@
     fclose(fp);
 
     //TODO should check this is a writable dir
     _certPath = certPath;
-    _certPathLen = sizeof(certPath);
+    _certPathLen = static_cast<int>(_certPath.length());
     //	_certLinks = certLinks;
     _certLinks = certPath; // temp to check if this works
+    // Keep the hot certificate set in memory.  The disk cache remains the
+    // persistent authority and this bounded cache only avoids repeated
+    // fopen/PEM parsing on subsequent TLS connections to popular hosts.
+    _certificateCacheLimit = 2048;
     //_ca_start = 1417872951;  // 6th Dec 2014
     //_ca_end = _ca_start + 315532800;  // 6th Dec 2024
     _ca_start = caStart;
     _ca_end = caEnd;
 }
+
+std::string CertificateAuthority::certificateCacheKey(const char *commonname, bool is_ip)
+{
+    std::string key(is_ip ? "I:" : "D:");
+    if (commonname == NULL)
+        return key;
+
+    key += commonname;
+    if (!is_ip) {
+        for (std::string::iterator character = key.begin() + 2;
+             character != key.end(); ++character) {
+            *character = static_cast<char>(std::tolower(
+                static_cast<unsigned char>(*character)));
+        }
+    }
+    return key;
+}
+
+bool CertificateAuthority::getCachedCertificate(const char *commonname, bool is_ip, X509 **cert)
+{
+    const std::string key = certificateCacheKey(commonname, is_ip);
+    std::lock_guard<std::mutex> lock(_certificateCacheMutex);
+    std::unordered_map<std::string, certificate_cache_entry>::iterator found =
+        _certificateCache.find(key);
+    if (found == _certificateCache.end())
+        return false;
+
+#if OPENSSL_VERSION_NUMBER < 0x10100000L
+    CRYPTO_add(&found->second.certificate->references, 1, CRYPTO_LOCK_X509);
+#else
+    if (X509_up_ref(found->second.certificate) != 1)
+        return false;
+#endif
+    _certificateCacheLru.splice(_certificateCacheLru.begin(),
+        _certificateCacheLru, found->second.lru_position);
+    found->second.lru_position = _certificateCacheLru.begin();
+    *cert = found->second.certificate;
+    return true;
+}
+
+void CertificateAuthority::cacheCertificate(const char *commonname, bool is_ip, X509 *cert)
+{
+    if (cert == NULL || _certificateCacheLimit == 0)
+        return;
+
+    const std::string key = certificateCacheKey(commonname, is_ip);
+    std::lock_guard<std::mutex> lock(_certificateCacheMutex);
+    std::unordered_map<std::string, certificate_cache_entry>::iterator found =
+        _certificateCache.find(key);
+    if (found != _certificateCache.end()) {
+        _certificateCacheLru.splice(_certificateCacheLru.begin(),
+            _certificateCacheLru, found->second.lru_position);
+        found->second.lru_position = _certificateCacheLru.begin();
+        return;
+    }
+
+#if OPENSSL_VERSION_NUMBER < 0x10100000L
+    CRYPTO_add(&cert->references, 1, CRYPTO_LOCK_X509);
+#else
+    if (X509_up_ref(cert) != 1)
+        return;
+#endif
+    _certificateCacheLru.push_front(key);
+    certificate_cache_entry entry;
+    entry.certificate = cert;
+    entry.lru_position = _certificateCacheLru.begin();
+    _certificateCache.insert(std::make_pair(key, entry));
+
+    while (_certificateCache.size() > _certificateCacheLimit) {
+        const std::string expired_key = _certificateCacheLru.back();
+        std::unordered_map<std::string, certificate_cache_entry>::iterator expired =
+            _certificateCache.find(expired_key);
+        if (expired != _certificateCache.end()) {
+            X509_free(expired->second.certificate);
+            _certificateCache.erase(expired);
+        }
+        _certificateCacheLru.pop_back();
+    }
+}
 
 bool CertificateAuthority::getSerial(const char *commonname, struct ca_serial *caser)
 {
@@ -192,7 +277,7 @@
 //write a certificate to disk being careful to avoid race conditions.
 //returns true if it already existed or false on error
 //common name (sh/c)ould be derived from the certificate but that would add to the complexity of the code
-bool CertificateAuthority::writeCertificate(const char *commonname, X509 *newCert, struct ca_serial *caser)
+bool CertificateAuthority::writeCertificate(const char *commonname, X509 *newCert, struct ca_serial *caser, bool is_ip)
 {
     std::string path(caser->filename);
     std::string dirpath(caser->filepath);
@@ -248,5 +333,6 @@
         fcntl(fd, F_SETLK, &fl);
         close(fd);
+        cacheCertificate(commonname, is_ip, newCert);
         return true;
     }
 
@@ -280,5 +366,5 @@
     fcntl(fd, F_SETLK, &fl);
     fclose(fp);
-    close(fd);
+    cacheCertificate(commonname, is_ip, newCert);
     return true;
 }
@@ -436,5 +522,8 @@
 bool CertificateAuthority::getServerCertificate(const char *commonname, X509 **cert, struct ca_serial *caser, bool is_ip)
 {
+    if (getCachedCertificate(commonname, is_ip, cert))
+        return true;
+
 
     getSerial(commonname, caser);
     std::string filename(caser->charhex);
@@ -463,6 +552,8 @@
         *cert = PEM_read_X509(link, NULL, NULL, NULL);
 
         fclose(link);
+        if (*cert != NULL)
+            cacheCertificate(commonname, is_ip, *cert);
 
         //dont need to check the return as this returns null if it couldnt load a cert
         return true;
@@ -548,4 +639,12 @@
 {
+    std::lock_guard<std::mutex> lock(_certificateCacheMutex);
+    for (std::unordered_map<std::string, certificate_cache_entry>::iterator entry =
+         _certificateCache.begin(); entry != _certificateCache.end(); ++entry) {
+        X509_free(entry->second.certificate);
+    }
+    _certificateCache.clear();
+    _certificateCacheLru.clear();
+
     if (_caCert) X509_free(_caCert);
     if (_caPrivKey) EVP_PKEY_free(_caPrivKey);
     if (_certPrivKey) EVP_PKEY_free(_certPrivKey);
