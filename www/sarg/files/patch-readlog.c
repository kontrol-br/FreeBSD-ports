--- readlog.c.orig	2020-01-16 10:35:42 UTC
+++ readlog.c
@@ -99,4 +99,5 @@ extern FileListObject AccessLog;
 extern const struct ReadLogProcessStruct ReadSquidLog;
 extern const struct ReadLogProcessStruct ReadCommonLog;
 extern const struct ReadLogProcessStruct ReadSargLog;
 extern const struct ReadLogProcessStruct ReadExtLog;
+extern const struct ReadLogProcessStruct ReadE2GuardianLog;
@@ -105,5 +106,6 @@ static const struct ReadLogProcessStruct * const LogFo
 {
 	&ReadSquidLog,
+	&ReadE2GuardianLog,
 	&ReadCommonLog,
 	&ReadSargLog,
 	&ReadExtLog
