--- authfail.c.orig	2020-01-16 10:35:42 UTC
+++ authfail.c
@@ -67,7 +67,9 @@ void authfail_write(const struct ReadLogStruct *log_en
 {
 	char date[80];
 
-	if (fp_authfail && (strstr(log_entry->HttpCode,"DENIED/401") != 0 || strstr(log_entry->HttpCode,"DENIED/407") != 0)) {
+	if (fp_authfail && (strstr(log_entry->HttpCode,"DENIED/401") != 0 ||
+	    strstr(log_entry->HttpCode,"DENIED/407") != 0 ||
+	    strcmp(log_entry->HttpCode,"401") == 0 || strcmp(log_entry->HttpCode,"407") == 0)) {
 		strftime(date,sizeof(date),"%d/%m/%Y\t%H:%M:%S",&log_entry->EntryTime);
 		fprintf(fp_authfail, "%s\t%s\t%s\t%s\n",date,log_entry->User,log_entry->Ip,log_entry->Url);
 		authfail_exists=true;
