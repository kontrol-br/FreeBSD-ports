--- src/CpuAffinitySet.cc.orig	2021-08-23 03:27:17 UTC
+++ src/CpuAffinitySet.cc
@@ -38,7 +38,7 @@
     } else {
         cpu_set_t cpuSet;
         memcpy(&cpuSet, &theCpuSet, sizeof(cpuSet));
-        (void) CPU_AND(&cpuSet, &cpuSet, &theOrigCpuSet);
+        CPU_AND(&cpuSet, &cpuSet, &theOrigCpuSet);
         if (CPU_COUNT(&cpuSet) <= 0) {
             debugs(54, DBG_IMPORTANT, "ERROR: invalid CPU affinity for process "
                    "PID " << getpid() << ", may be caused by an invalid core in "
