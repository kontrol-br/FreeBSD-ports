--- denied.c.orig	2020-01-16 10:35:42 UTC
+++ denied.c
@@ -68,7 +68,8 @@ void denied_write(const struct ReadLogStruct *log_entr
 {
 	char date[80];
 
-	if (fp_denied && strstr(log_entry->HttpCode,"DENIED/403") != 0) {
+	if (fp_denied && (strstr(log_entry->HttpCode,"DENIED/403") != 0 ||
+	    strcmp(log_entry->HttpCode,"403") == 0)) {
 		strftime(date,sizeof(date),"%d/%m/%Y\t%H:%M:%S",&log_entry->EntryTime);
 		fprintf(fp_denied, "%s\t%s\t%s\t%s\n",date,log_entry->User,log_entry->Ip,log_entry->Url);
 		denied_exists=true;
