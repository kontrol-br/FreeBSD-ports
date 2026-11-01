--- src/CertificateAuthority.hpp.orig
+++ src/CertificateAuthority.hpp
@@ -2,6 +2,11 @@
 #define __HPP_CERTIFICATEAUTHORITY
 #ifdef __SSLMITM
 
+#include <list>
+#include <mutex>
+#include <string>
+#include <unordered_map>
+
 struct ca_serial {
     ASN1_INTEGER *asn;
     char *charhex;
@@ -13,6 +18,11 @@
 class CertificateAuthority
 {
 
+    struct certificate_cache_entry {
+        X509 *certificate;
+        std::list<std::string>::iterator lru_position;
+    };
+
     protected:
     EVP_PKEY *_caPrivKey;
     EVP_PKEY *_certPrivKey;
@@ -24,6 +34,13 @@
     static int do_mkdir(const char *path, mode_t mode);
     int mkpath(const char *path, mode_t mode);
     bool addExtension(X509 *cert, int nid, char *value);
+    std::string certificateCacheKey(const char *commonname, bool is_ip);
+    bool getCachedCertificate(const char *commonname, bool is_ip, X509 **cert);
+    void cacheCertificate(const char *commonname, bool is_ip, X509 *cert);
+    std::mutex _certificateCacheMutex;
+    std::list<std::string> _certificateCacheLru;
+    std::unordered_map<std::string, certificate_cache_entry> _certificateCache;
+    size_t _certificateCacheLimit;
 
     public:
     CertificateAuthority(const char *caCert,
@@ -39,7 +56,7 @@
     X509 *generateCertificate(const char *commonname, struct ca_serial *cser, bool is_ip = false);
     bool getSerial(const char *commonname, struct ca_serial *cser);
     bool getServerCertificate(const char *commonname, X509 **cert, struct ca_serial *cser,bool is_ip = false);
-    bool writeCertificate(const char *hostname, X509 *newCert, struct ca_serial *cser);
+    bool writeCertificate(const char *hostname, X509 *newCert, struct ca_serial *cser, bool is_ip = false);
     EVP_PKEY *getServerPkey();
     bool free_ca_serial(struct ca_serial *cs);
 };
