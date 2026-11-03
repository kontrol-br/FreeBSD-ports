--- src/ConnectionHandler.cpp.orig
+++ src/ConnectionHandler.cpp
@@ -894,6 +894,7 @@
             DataBuffer docbody;
             docbody.setTimeout(o.exchange_timeout);
             FDTunnel fdt;
+            bool protocolUpgrade = false;
 
             if (firsttime) {
                 // reset flags & objects next time round the loop
@@ -1162,6 +1163,16 @@
             if (checkme.special_exception_ip) {
                 checkme.nomitm = true;
                 checkme.automitm = false;
+            } else if (checkme.special_banned_ip) {
+                filtergroup = -1;
+                checkme.filtergroup = -1;
+                checkme.isBlocked = true;
+                checkme.isItNaughty = true;
+                if (checkme.message_no == 0)
+                    checkme.message_no = 100;
+                if (checkme.log_message_no == 0)
+                    checkme.log_message_no = 103;
+                authed = true;
             }
 
 #ifdef E2DEBUG
@@ -1183,9 +1194,15 @@
             //			Set if candidate for MITM
             //			(Exceptions will not go MITM)
             FOptionContainer *fg_cand = currentFG();
+            if (!fg_cand && checkme.special_banned_ip) {
+                fg_cand = getFG(header.isProxyRequest ? o.default_fg : o.default_trans_fg);
+                if (!fg_cand)
+                    fg_cand = getFG(0);
+            }
             checkme.ismitmcandidate = checkme.isconnect && (!checkme.nomitm) && (fg_cand && fg_cand->ssl_mitm) && (header.port == 443);
             if (checkme.ismitmcandidate ) {
                 if(fg_cand && !fg_cand->automitm) checkme.automitm = false;
+                else if (checkme.special_banned_ip && fg_cand && fg_cand->automitm) checkme.automitm = true;
             } else {
                 checkme.nomitm = true;
                 checkme.automitm = false;
@@ -1435,6 +1452,16 @@
                     //check response code
                     if ((!checkme.isItNaughty) && (!checkme.upfailure)) {
                         int rcode = docheader.returnCode();
+                        protocolUpgrade = header.isProtocolUpgrade()
+                                          && docheader.isProtocolUpgrade()
+                                          && rcode == 101;
+                        if (protocolUpgrade) {
+                            // The HTTP exchange ends at 101.  The remaining
+                            // bytes belong to the upgraded bidirectional
+                            // protocol and the connection cannot be reused.
+                            persistProxy = false;
+                            persistPeer = false;
+                        }
                         if (rcode == 407) {   // proxy auth required
                             // tunnel thru -  may be content
                             checkme.tunnel_rest = true;
@@ -1458,7 +1485,7 @@
                             }
                         }
 
-                        if (docheader.contentLength() == 0)   // no content
+                        if (!protocolUpgrade && docheader.contentLength() == 0)   // no content
                             checkme.tunnel_rest = true;
 
                     }
@@ -1486,6 +1513,13 @@
                 }
             }
 
+            // Apply this after exception handling because an HTTP Upgrade is
+            // neither a response body nor a reusable persistent connection.
+            if (protocolUpgrade) {
+                checkme.tunnel_rest = false;
+                checkme.tunnel_2way = true;
+            }
+
 #ifdef __SSLMITM
             //if ismitm - GO MITM
             // ssl_grey is covered in storyboard
@@ -1507,7 +1541,7 @@
 
             //CALL SB checkresponse
             FOptionContainer *fg_resp = currentFG();
-            if (fg_resp && (!checkme.isItNaughty) && (!checkme.upfailure) && (!checkme.isconnect) && (!checkme.logcategory) && !checkme.tunnel_rest) {
+            if (fg_resp && (!checkme.isItNaughty) && (!checkme.upfailure) && (!checkme.isconnect) && (!checkme.logcategory) && !checkme.tunnel_rest && !protocolUpgrade) {
                 fg_resp->StoryB.runFunctEntry(ENT_STORYB_PROXY_RESPONSE, checkme);
 #ifdef E2DEBUG
                 std::cerr << thread_id << "After StoryB checkresponse " << checkme.isexception << " mess_no "
@@ -1668,7 +1702,7 @@
         fg_log->StoryB.runFunctEntry(ENT_STORYB_LOG_CHECK, cm);
     if(cm.nolog) return;
 
-    if(o.log_requests) {
+    if(o.log_requests && !o.rqlog_only_tunnel_open) {
         what = thread_id;
     }
     what += cm.whatIsNaughtyLog;
@@ -1836,10 +1870,12 @@
 }
 
 void ConnectionHandler::doRQLog(std::string &who, std::string &from, NaughtyFilter &cm, std::string &funct) {
+    if (o.rqlog_only_tunnel_open && funct != "THTTPS_OPEN")
+        return;
     String rtype = cm.request_header->requestType();
     String where = cm.logurl;
     unsigned int port = cm.request_header->port;
-    std::string what = thread_id;
+    std::string what = o.rqlog_only_tunnel_open ? "" : thread_id;
     what += funct;
     if (cm.isTLS)
         what += "_TLS";
@@ -2175,12 +2211,6 @@
                             }
                         }
 
-                        if (headersent == 0) {
-                            eheader = "HTTP/1.1 200 \r\n";
-                        }
-                        if (headersent < 2) {
-                            eheader += "Content-type: text/html\r\n\r\n";
-                        }
                         // if the header has been sent then likely displaying the
                         // template will break the download, however as this is
                         // only going to be happening if the unsafe trickle
@@ -2204,6 +2234,16 @@
                                                                                               &(checkme->clienthost), filtergroup,
                                                                                               fg_ptr->name,
                                                                                               hashed, localip, flags );
+                        if (headersent == 0) {
+                            eheader = "HTTP/1.1 200 OK\r\n";
+                        }
+                        if (headersent < 2) {
+                            eheader += "Content-Type: text/html\r\n";
+                            eheader += "Connection: close\r\n";
+                            eheader += "Content-Length: ";
+                            eheader += std::to_string(ebody.length());
+                            eheader += "\r\n\r\n";
+                        }
                     }
                 }
             }
@@ -3015,11 +3055,14 @@
         std::cerr << thread_id << " nf " << checkme.isItNaughty <<
                 " upfail " << checkme.upfailure << std::endl;
 #endif
-    if ((!checkme.isItNaughty) && (!checkme.upfailure)) {
+    bool handled_in_tunnel = false;
+    if (((!checkme.isItNaughty) && (!checkme.upfailure)) ||
+        (checkme.special_banned_ip && !justLog && !checkme.upfailure)) {
+        handled_in_tunnel = true;
         bool writecert = true;
         if (!certfromcache) {
             writecert = o.ca->writeCertificate(checkme.urldomain.c_str(), cert,
-                                               &caser);
+                                               &caser, checkme.isiphost);
         }
 
 //if we cant write the certificate its not the end of the world but it is slow
@@ -3033,7 +3076,7 @@
         std::cerr << thread_id << " -Handling connections inside ssl tunnel" << std::endl;
 #endif
 
-        if (authed) {
+        if (authed && !checkme.special_banned_ip) {
             persistent_authed = true;
         }
 
@@ -3047,17 +3090,23 @@
 
 //stopssl on the proxy connection
 //if it was marked as naughty then show a deny page and close the connection
-    if (checkme.isItNaughty || checkme.upfailure) {
+    if (!handled_in_tunnel && (checkme.isItNaughty || checkme.upfailure)) {
 #ifdef E2DEBUG
         std::cerr << thread_id << " -SSL Interception failed " << checkme.whatIsNaughty << " nf " << checkme.isItNaughty <<
                 " upfail " << checkme.upfailure << std::endl;
 #endif
         doLog(clientuser, clientip, checkme);
 
-        if(!justLog)
-        	denyAccess(&peerconn, &proxysock, header, docheader, &checkme.logurl, &checkme, &clientuser,
+        if(!justLog) {
+            HTTPHeader mitm_header(__HEADER_REQUEST);
+            mitm_header.setTimeout(o.pcon_timeout);
+            if (mitm_header.in(&peerconn, false)) {
+                header = &mitm_header;
+            }
+            denyAccess(&peerconn, &proxysock, header, docheader, &checkme.logurl, &checkme, &clientuser,
                    &clientip, filtergroup, checkme.ispostblock, checkme.headersent, checkme.wasinfected,
                    checkme.scanerror, checkme.badcert);
+        }
     }
 #ifdef E2DEBUG
     std::cerr << thread_id << " -Shutting down ssl to proxy" << std::endl;
@@ -3608,7 +3657,7 @@
 
             filtergroup = o.default_trans_fg;
 
-            if(o.log_requests) {
+            if(o.log_requests && !o.rqlog_only_tunnel_open) {
                 std::string fnt = "THTTPS";
                 doRQLog(clientuser, clientip, checkme, fnt);
             }
@@ -3652,8 +3701,21 @@
                      }
                 }
                 checkme.filtergroup = filtergroup;
-                FOptionContainer *fg_trans = currentFG();
-                if(!checkme.nomitm) checkme.nomitm = (fg_trans == nullptr || !fg_trans->ssl_mitm);
+                if (checkme.special_banned_ip) {
+                    filtergroup = -1;
+                    checkme.filtergroup = -1;
+                    checkme.isBlocked = true;
+                    checkme.isItNaughty = true;
+                    isbannedip = true;
+                    if (checkme.message_no == 0)
+                        checkme.message_no = 100;
+                    if (checkme.log_message_no == 0)
+                        checkme.log_message_no = 103;
+                    authed = true;
+                } else {
+                    FOptionContainer *fg_trans = currentFG();
+                    if(!checkme.nomitm) checkme.nomitm = (fg_trans == nullptr || !fg_trans->ssl_mitm);
+                }
             }
 
 #ifdef E2DEBUG
@@ -3676,6 +3738,11 @@
             checkme.clientip = clientip;
 
             FOptionContainer *fg_trans = currentFG();
+            if (!fg_trans && checkme.special_banned_ip) {
+                fg_trans = getFG(o.default_trans_fg);
+                if (!fg_trans)
+                    fg_trans = getFG(0);
+            }
             if(fg_trans && checkme.hasSNI && !checkme.nomitm ) checkme.ismitmcandidate = fg_trans->ssl_mitm;
             if(fg_trans && checkme.ismitmcandidate) {
                 checkme.automitm = fg_trans->automitm;
@@ -3784,6 +3851,11 @@
 #ifdef E2DEBUG
                 std::cerr << thread_id << " -Tunnelling to client" << std::endl;
 #endif
+                if (o.log_requests && o.rqlog_only_tunnel_open) {
+                    std::string fnt = "THTTPS_OPEN";
+                    doRQLog(clientuser, clientip, checkme, fnt);
+                }
+
                 if (!fdt.tunnel(proxysock, peerconn,true, -1 , true))
                     persistProxy = false;
                 checkme.docsize += fdt.throughput;
@@ -4736,10 +4808,14 @@
             persistPeer = false;
         }
 
-            //Log
-    if (!checkme.isourwebserver && checkme.isItNaughty) { // don't log requests to the web server & and normal response
-        checkme.whatIsNaughtyLog = "ICAP Response filtering: ";
-        checkme.whatIsNaughtyLog += checkme.whatIsNaughty;
+    // REQMOD deliberately defers clean-request logging to RESPMOD so the
+    // final status, size and elapsed time are available.  Log every normal
+    // response here; blocked requests have already been logged by REQMOD.
+    if (!checkme.isourwebserver) {
+        if (checkme.isItNaughty) {
+            checkme.whatIsNaughtyLog = "ICAP Response filtering: ";
+            checkme.whatIsNaughtyLog += checkme.whatIsNaughty;
+        }
         doLog(clientuser, clientip, checkme);
     }
     if (persistPeer)
