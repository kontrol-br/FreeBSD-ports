/*
 * Native E2Guardian access.log reader for SARG.
 *
 * Copyright (C) 2026 Kontrol Tecnologia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "include/conf.h"
#include "include/defs.h"
#include "include/readlog.h"

#define E2G_MAX_FIELDS 64

static void E2Guardian_NewFile(const char *FileName)
{
	(void)FileName;
}

static bool E2Guardian_IsInteger(const char *Text)
{
	if (!Text || !*Text)
		return(false);
	if (*Text == '-')
		Text++;
	if (!*Text)
		return(false);
	while (*Text) {
		if (!isdigit((unsigned char)*Text))
			return(false);
		Text++;
	}
	return(true);
}

static bool E2Guardian_IsTimestamp(const char *Text)
{
	bool dot=false;

	if (!Text || !isdigit((unsigned char)*Text))
		return(false);
	while (*Text) {
		if (*Text == '.' && !dot) {
			dot=true;
		} else if (!isdigit((unsigned char)*Text)) {
			return(false);
		}
		Text++;
	}
	return(dot);
}

/*
 * Log readers are tried in sequence against the same mutable input buffer.
 * Reject Squid/common/Sarg records before splitting the line so the next
 * reader always receives the original content.
 */
static bool E2Guardian_HasHumanTimestamp(const char *Line)
{
	const unsigned char *text=(const unsigned char *)Line;

	if (!text)
		return(false);
	if (*text == '"')
		text++;
	if (strlen((const char *)text) < 19)
		return(false);
	return(isdigit(text[0]) && isdigit(text[1]) &&
	       isdigit(text[2]) && isdigit(text[3]) && text[4] == '.' &&
	       isdigit(text[5]) && isdigit(text[6]) && text[7] == '.' &&
	       isdigit(text[8]) && isdigit(text[9]) && isspace(text[10]) &&
	       isdigit(text[11]) && isdigit(text[12]) && text[13] == ':' &&
	       isdigit(text[14]) && isdigit(text[15]) && text[16] == ':' &&
	       isdigit(text[17]) && isdigit(text[18]));
}

static bool E2Guardian_IsMethod(const char *Text)
{
	static const char * const Methods[]={
		"GET", "POST", "CONNECT", "HEAD", "OPTIONS", "PUT", "DELETE",
		"PATCH", "TRACE", "PROPFIND", "PROPPATCH", "MKCOL", "COPY",
		"MOVE", "LOCK", "UNLOCK", "-", NULL
	};
	int i;

	for (i=0 ; Methods[i] ; i++)
		if (strcmp(Text,Methods[i]) == 0)
			return(true);
	return(false);
}

static bool E2Guardian_IsHttpStatus(const char *Text)
{
	long status;
	char *end;

	if (!E2Guardian_IsInteger(Text))
		return(false);
	status=strtol(Text,&end,10);
	return(*end == '\0' && (status == 0 || (status >= 100 && status <= 599)));
}

static bool E2Guardian_ParseHumanTime(const char *Date,const char *Time,struct tm *Result)
{
	int year,month,day,hour,minute,second;

	if (sscanf(Date,"%d.%d.%d",&year,&month,&day) != 3 ||
	    sscanf(Time,"%d:%d:%d",&hour,&minute,&second) != 3)
		return(false);
	if (year < 1970 || year > 2200 || month < 1 || month > 12 ||
	    day < 1 || day > 31 || hour < 0 || hour > 23 ||
	    minute < 0 || minute > 59 || second < 0 || second > 60)
		return(false);

	memset(Result,0,sizeof(*Result));
	Result->tm_year=year-1900;
	Result->tm_mon=month-1;
	Result->tm_mday=day;
	Result->tm_hour=hour;
	Result->tm_min=minute;
	Result->tm_sec=second;
	Result->tm_isdst=-1;
	return(mktime(Result) != (time_t)-1);
}

static int E2Guardian_SplitWhitespace(char *Line,char **Fields,int Maximum)
{
	int count=0;

	while (*Line) {
		while (*Line && isspace((unsigned char)*Line))
			Line++;
		if (!*Line)
			break;
		if (count >= Maximum)
			return(-1);
		Fields[count++]=Line;
		while (*Line && !isspace((unsigned char)*Line))
			Line++;
		if (*Line)
			*Line++='\0';
	}
	return(count);
}

static int E2Guardian_SplitTabs(char *Line,char **Fields,int Maximum)
{
	int count=0;

	if (Maximum <= 0)
		return(-1);
	Fields[count++]=Line;
	while (*Line) {
		if (*Line == '\t') {
			*Line='\0';
			if (count >= Maximum)
				return(-1);
			Fields[count++]=Line+1;
		}
		Line++;
	}
	return(count);
}

static int E2Guardian_SplitCsv(char *Line,char **Fields,int Maximum)
{
	char *source=Line;
	char *destination=Line;
	int count=0;
	bool quoted;

	do {
		if (count >= Maximum)
			return(-1);
		Fields[count++]=destination;
		quoted=(*source == '"');
		if (quoted)
			source++;
		while (*source) {
			if (quoted) {
				if (*source == '"') {
					if (source[1] == '"') {
						*destination++='"';
						source+=2;
						continue;
					}
					source++;
					if (*source && *source != ',')
						return(-1);
					break;
				}
			} else if (*source == ',') {
				break;
			}
			*destination++=*source++;
		}
		*destination++='\0';
		if (*source == ',')
			source++;
		else if (*source)
			return(-1);
	} while (*source);
	return(count);
}

static enum ReadLogReturnCodeEnum E2Guardian_ReadDelimited(char *Line,struct ReadLogStruct *Entry,bool Csv)
{
	char *fields[E2G_MAX_FIELDS];
	char date[16],time[16];
	int count;

	count=Csv ? E2Guardian_SplitCsv(Line,fields,E2G_MAX_FIELDS) :
	            E2Guardian_SplitTabs(Line,fields,E2G_MAX_FIELDS);
	if (count < 1)
		return(RLRC_Unknown);

	/* Formats 2 and 4 have the same columns. Their first column contains
	 * the readable date and time separated by one space. */
	if (strchr(fields[0],' ') != NULL) {
		if (sscanf(fields[0],"%15s %15s",date,time) != 2)
			return(RLRC_Unknown);
		if (count < 19 || !E2Guardian_ParseHumanTime(date,time,&Entry->EntryTime))
			return(RLRC_Unknown);
		Entry->User=fields[1];
		Entry->Ip=fields[2];
		Entry->Url=fields[3];
		Entry->HttpMethod=fields[5];
		if (!E2Guardian_IsMethod(Entry->HttpMethod) || !E2Guardian_IsInteger(fields[6]) ||
		    !E2Guardian_IsInteger(fields[9]) || !E2Guardian_IsInteger(fields[10]))
			return(RLRC_Unknown);
		Entry->DataSize=strtoll(fields[6],NULL,10);
		Entry->ElapsedTime=0LL;
		Entry->HttpCode=fields[10];
		Entry->UserAgent=NULL;
		return(RLRC_NoError);
	}

	return(RLRC_Unknown);
}

static enum ReadLogReturnCodeEnum E2Guardian_ReadNative(char *Line,struct ReadLogStruct *Entry)
{
	char *fields[E2G_MAX_FIELDS];
	int count;
	int base=0;
	int method;
	int status=-1;
	int field;

	count=E2Guardian_SplitWhitespace(Line,fields,E2G_MAX_FIELDS);
	if (count < 14 || !E2Guardian_ParseHumanTime(fields[0],fields[1],&Entry->EntryTime))
		return(RLRC_Unknown);
	if (count > 2 && E2Guardian_IsTimestamp(fields[2]))
		base=1;
	if (count < 14+base)
		return(RLRC_Unknown);

	for (method=5+base ; method+2<count ; method++) {
		if (E2Guardian_IsMethod(fields[method]) &&
		    E2Guardian_IsInteger(fields[method+1]) &&
		    E2Guardian_IsInteger(fields[method+2]))
			break;
	}
	if (method+2 >= count)
		return(RLRC_Unknown);

	/* The category may contain spaces. The last adjacent numeric pair after
	 * the byte counters is filter-group/status; scanning from the method and
	 * retaining the last match avoids treating bytes/elapsed as HTTP status. */
	for (field=method+3 ; field+1<count ; field++) {
		if (E2Guardian_IsInteger(fields[field]) &&
		    E2Guardian_IsHttpStatus(fields[field+1]))
			status=field+1;
	}
	if (status < 0)
		return(RLRC_Unknown);

	Entry->User=fields[2+base];
	Entry->Ip=fields[3+base];
	Entry->Url=fields[4+base];
	Entry->HttpMethod=fields[method];
	Entry->DataSize=strtoll(fields[method+1],NULL,10);
	Entry->ElapsedTime=0LL;
	Entry->HttpCode=fields[status];
	Entry->UserAgent=NULL;
	return(RLRC_NoError);
}

static enum ReadLogReturnCodeEnum E2Guardian_ReadEntry(char *Line,struct ReadLogStruct *Entry)
{
	if (!Line || !*Line || !E2Guardian_HasHumanTimestamp(Line))
		return(RLRC_Unknown);
	if (strchr(Line,'\t') != NULL)
		return(E2Guardian_ReadDelimited(Line,Entry,false));
	if (*Line == '"')
		return(E2Guardian_ReadDelimited(Line,Entry,true));
	return(E2Guardian_ReadNative(Line,Entry));
}

const struct ReadLogProcessStruct ReadE2GuardianLog=
{
	N_("Kontrol-ID/E2Guardian native log format"),
	E2Guardian_NewFile,
	E2Guardian_ReadEntry
};
