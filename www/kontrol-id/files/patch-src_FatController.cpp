--- src/FatController.cpp.orig
+++ src/FatController.cpp
@@ -254,7 +254,10 @@ void stat_rec::reset()
         bc, o.http_worker_Q.size(), o.log_Q->size(), cnx, cps, rqx, rqs, mfd, LC);
     }
 
-    fflush(fs);
+    if (fflush(fs) != 0) {
+        syslog(LOG_ERR, "Kontrol-ID stats log write failed: %s", strerror(errno));
+        o.dstat_log_flag = false;
+    }
 };
 
 void stat_rec::close()
@@ -756,6 +759,7 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
     int headeradded = 0;
 
     std::ofstream *logfile = NULL;
+    std::streamoff log_bytes = 0;
     if (!logsyslog) {
         logfile = new std::ofstream(log_location.c_str(), std::ios::app);
         if (logfile->fail()) {
@@ -772,6 +776,10 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
 
         }
     }
+    if (is_RQlog && o.rqlog_only_tunnel_open && logfile) {
+        std::streampos current_position = logfile->tellp();
+        log_bytes = current_position > 0 ? static_cast<std::streamoff>(current_position) : 0;
+    }
 
     String server("");
     // Get server name - only needed for formats 5 & 7
@@ -1068,7 +1076,7 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
             groupname = is_RQlog ? "" : "none";
         } else {
             stringgroup = String(filtergroup + 1);
-            if (is_RQlog) {
+            if (is_RQlog && !o.rqlog_only_tunnel_open) {
                 groupname = "";
             } else {
                 if (stringcode == "407") {
@@ -1186,10 +1194,28 @@ void log_listener(std::string log_location, bool is_RQlog, bool logsyslog, Queue
             builtline += flags;
         }
 
-        if (!logsyslog)
+        if (!logsyslog) {
+            if (is_RQlog && o.rqlog_only_tunnel_open && o.rqlog_max_bytes > 0 &&
+                log_bytes + static_cast<std::streamoff>(builtline.size() + 1) > o.rqlog_max_bytes) {
+                logfile->close();
+                logfile->open(log_location.c_str(), std::ios::out | std::ios::trunc);
+                if (logfile->fail()) {
+                    syslog(LOG_ERR, "%sError truncating bounded Real Time log file.", thread_id.c_str());
+                    return;
+                }
+                log_bytes = 0;
+            }
             *logfile << builtline << std::endl; // append the line
-        else
+            if (logfile->fail()) {
+                syslog(LOG_CRIT, "%sAccess log write failed; stopping the engine", thread_id.c_str());
+                return;
+            }
+            if (is_RQlog && o.rqlog_only_tunnel_open) {
+                log_bytes += static_cast<std::streamoff>(builtline.size() + 1);
+            }
+        } else {
             syslog(LOG_INFO, "%s", builtline.c_str());
+        }
 #ifdef E2DEBUG
         std::cerr << itemcount << " " << builtline << std::endl;
 #endif
@@ -1632,8 +1658,8 @@ int fc_controlit()   //
         return 1; // seteuid failed for some reason so exit with error
     }
 
-    if (serversockets.listenAll(256)) { // set it to listen mode with a kernel
-        // queue of 256 backlog connections
+    if (serversockets.listenAll(4096)) { // the kernel clamps this to somaxconn
+        // A busy gateway needs room for short bursts during worker dispatch.
         if (!is_daemonised) {
             std::cerr << thread_id << "Error listening to server socket" << std::endl;
         }
@@ -1696,19 +1722,36 @@ int fc_controlit()   //
     // This removes need for select and/or epoll greatly simplifying the code
     // Threads are created for logger, a separate thread for each listening port
     // and an array of worker threads to deal with the work.
+    auto fail_thread_start = [&]() {
+        ttg = true;
+        for (int j = 0; j < serversocketcount; ++j)
+            serversockets[j]->close();
+        syslog(LOG_CRIT, "%sCritical startup failure: required thread unavailable; listeners closed", thread_id.c_str());
+        // Other detached workers may already be waiting on global queues.
+        // Exit without destroying those queues while they are still in use.
+        _exit(1);
+    };
     if (!o.no_logger) {
-        spawn_detached_thread([=]() {
+        if (!spawn_detached_thread([=]() {
             log_listener(o.log_location, false, o.log_syslog, o.log_Q);
-        }, "logger");
+            if (!logger_ttg) {
+                syslog(LOG_CRIT, "Kontrol-ID access logger exited unexpectedly");
+                _exit(1);
+            }
+        }, "logger")) fail_thread_start();
 #ifdef E2DEBUG
     std::cerr << thread_id << "log_listener thread created" << std::endl;
 #endif
     }
 
     if(o.log_requests) {
-        spawn_detached_thread([=]() {
+        if (!spawn_detached_thread([=]() {
             log_listener(o.RQlog_location, true, false, o.RQlog_Q);
-        }, "request logger");
+            if (!logger_ttg) {
+                syslog(LOG_CRIT, "Kontrol-ID request logger exited unexpectedly");
+                _exit(1);
+            }
+        }, "request logger")) fail_thread_start();
 #ifdef E2DEBUG
         std::cerr << thread_id << "RQlog_listener thread created" << std::endl;
 #endif
@@ -1759,9 +1802,13 @@ int fc_controlit()   //
     // worker thread generation
     int i;
     for (i = 0; i < o.http_workers; i++) {
-        spawn_detached_thread([i]() {
+        if (!spawn_detached_thread([i]() {
             handle_connections(i);
-        }, "http worker");
+            if (!ttg) {
+                syslog(LOG_CRIT, "Kontrol-ID HTTP worker exited unexpectedly");
+                _exit(1);
+            }
+        }, "http worker")) fail_thread_start();
     }
 #ifdef E2DEBUG
     std::cerr << thread_id << "http_worker threads created" << std::endl;
@@ -1770,9 +1817,13 @@ int fc_controlit()   //
     //   set listener threads going
 
     for (int i = 0; i < serversocketcount; i++) {
-        spawn_detached_thread([i]() {
+        if (!spawn_detached_thread([i]() {
             accept_connections(i);
-        }, "listener");
+            if (!ttg) {
+                syslog(LOG_CRIT, "Kontrol-ID listener exited unexpectedly");
+                _exit(1);
+            }
+        }, "listener")) fail_thread_start();
     }
 #ifdef E2DEBUG
     std::cerr << "listen  threads created" << std::endl;
