/*
   rcfile.c

   This file is part of GNU Anubis.
   Copyright (C) 2001, 2002, 2003, 2004 The Anubis Team.

   GNU Anubis is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GNU Anubis is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Anubis; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   GNU Anubis is released under the GPL with the additional exemption that
   compiling, linking, and/or using OpenSSL is allowed.
*/

#include "headers.h"
#include "extern.h"
#include "rcfile.h"

#define setbool(a, b, c) \
   do {\
        if (strcasecmp("yes", a) == 0) \
		(b) |= (c); \
	else if (strcasecmp("no", a) == 0) \
		(b) &= ~(c); \
        else \
                return RC_KW_ERROR; \
   } while (0)

#define if_empty_set(a, b, c) \
   do {\
	if (strlen(a) == 0) { \
		(b) &= ~(c); \
	} else { \
		(b) |= (c); \
	}\
   } while (0)		 

#define MAX_SECTIONS 10

static RC_SECTION *parse_tree;
static time_t global_mtime;
static struct rc_secdef anubis_rc_sections[MAX_SECTIONS];
static int anubis_rc_numsections;

struct rc_secdef *
anubis_add_section(char *name)
{
	int i;
	
	if (anubis_rc_numsections >= MAX_SECTIONS)
		abort(); /*FIXME*/
	
	for (i = 0; i < anubis_rc_numsections; i++)
		if (strcmp(anubis_rc_sections[i].name, name) == 0)
			return &anubis_rc_sections[i];
	
	anubis_rc_sections[anubis_rc_numsections].name = name;
	anubis_rc_sections[anubis_rc_numsections].allow_prog = 0;
	anubis_rc_sections[anubis_rc_numsections].prio = prio_user_only;
	anubis_rc_sections[anubis_rc_numsections].child = NULL;
	return &anubis_rc_sections[anubis_rc_numsections++];
}

struct rc_secdef *
anubis_find_section(char *name)
{
	int i;
	for (i = 0; i < anubis_rc_numsections; i++)
		if (strcmp(anubis_rc_sections[i].name, name) == 0)
			return &anubis_rc_sections[i];
	return NULL;
}

void
anubis_section_set_prio(char *name, enum section_prio prio)
{
	struct rc_secdef *p = anubis_find_section(name);
	if (p)
		p->prio = prio;
}

/* A structure uniquely identifying a file */
struct file_id {
	dev_t dev;      /* Device number */
	ino_t ino;      /* I-node number */
};

/* A list of struct file_id used to prevent duplicate parsing of the
   same file */
static LIST *file_id_list;

/* Comparator for two struct file_id */
static int
cmp_fid(void *a, void *b)
{
	struct file_id *fid_a = a,
	               *fid_b = b;
	return !(fid_a->dev == fid_b->dev && fid_a->ino == fid_b->ino);
}

/* Adds the `filename' to file_id_list.
   Returns 0 if the operation passed successfully, 1 -- if the file
   is already present in the list, and -1 on error */
static int
file_id_add (char *filename)
{
	struct stat st;
	struct file_id *fid;
	if (stat(filename, &st)) {
		anubis_error(SOFT,
			     _("cannot stat file `%s': %s"), filename,
			     strerror(errno));
		return -1;
	}
	fid = xmalloc(sizeof(*fid));
	fid->dev = st.st_dev;
	fid->ino = st.st_ino;
	if (list_locate(file_id_list, fid, cmp_fid)) {
		free(fid);
		info(DEBUG,
		     _("File `%s' has already been read"),
		     filename);
		return 1;
	}
	if (!file_id_list)
		file_id_list = list_create();
	list_append(file_id_list, fid);
	return 0;
}

static void
file_id_destroy()
{
	list_destroy(&file_id_list, anubis_free_list_item, NULL);
}

void
open_rcfile(int method)
{
	char homedir[MAXPATHLEN+1];
	char *rcfile = 0;
	RC_SECTION *sec;

	switch (method) {
	case CF_SUPERVISOR:
	case CF_INIT:
		if (topt & T_ALTRC) {
			rcfile = strdup(options.altrc);
		} else if (check_superuser())
			rcfile = strdup(DEFAULT_GLOBAL_RCFILE);
		else {
			get_homedir(session.supervisor,
				    homedir, sizeof(homedir));
			rcfile = xmalloc(strlen(homedir) +
					 strlen(DEFAULT_LOCAL_RCFILE) + 2);
			sprintf(rcfile,	"%s/%s", homedir,
				DEFAULT_LOCAL_RCFILE);
		}

		if (check_filename(rcfile, &global_mtime) == 0) {
			free(rcfile);
			return;
		}
		rc_section_list_destroy(&parse_tree);
		file_id_destroy();
		info(DEBUG,
		     _("Reading system configuration file %s..."), rcfile);
		break;

	case CF_CLIENT:
		if ((topt & (T_ALTRC|T_NORC)) == (T_ALTRC|T_NORC)) {
			rcfile = strdup(options.altrc);
		} else {
			rcfile = user_rcfile_name();
		}
		info(DEBUG,
		     _("Reading user configuration file %s..."), rcfile);
	}

	if ((topt & T_RELAX_PERM_CHECK) == 0
	    && check_filemode(rcfile) == 0) { /* Wrong permissions... */
		free(rcfile);
		return;
	}

	if (file_id_add(rcfile) == 0) {
		sec = rc_parse(rcfile);
		if (sec)
			rc_section_link(&parse_tree, sec);
	}
	free(rcfile);
}

void
process_rcfile(int method)
{
	rcfile_process_section(method, "CONTROL", NULL, NULL);
#ifdef WITH_GUILE
	rcfile_process_section(method, "GUILE", NULL, NULL);
#endif
#if defined(WITH_GSASL)
	rcfile_process_section(method, "AUTH", NULL, NULL);
#endif
}

/* ************************** The CONTROL Section ************************* */ 
#define KW_BIND                 0
#define KW_TERMLEVEL            1
#define KW_LOGLEVEL             2
#define KW_LOGFILE              3
#define KW_TRACEFILE            4
#define KW_REMOTE_MTA           5
#define KW_LOCAL_MTA            6
#define KW_RULE_PRIORITY        7
#define KW_CONTROL_PRIORITY     8
#define KW_ESMTP_AUTH           9
#define KW_DROP_UNKNOWN_USER   10
#define KW_USER_NOTPRIVILEGED  11
#define KW_ALLOW_LOCAL_MTA     12
#define KW_SOCKS_PROXY         13
#define KW_SOCKS_V4            14
#define KW_SOCKS_AUTH          15
#define KW_READ_ENTIRE_BODY    16
#define KW_LOCAL_DOMAIN        17
#define KW_MODE                18

char **
list_to_argv(LIST *list)
{
	int i, argc;
	char **argv, *p;
	ITERATOR *itr;

	argc = list_count(list);
	argv = xmalloc((argc + 1) * sizeof(argv[0]));
	itr = iterator_create(list);
	for (i = 0, p = iterator_first(itr); p; i++, p = iterator_next(itr))
		argv[i] = strdup(p);
	iterator_destroy(&itr);
	argv[i] = NULL;
	return argv;
}

int
control_parser(int method, int key, LIST *arglist,
	       void *inv_data, void *func_data, MESSAGE *msg)
{
	char *arg = list_item(arglist, 0);
	
	switch (key) {
	case KW_BIND:                
		parse_mtahost(arg, session.anubis, &session.anubis_port);
		if (strlen(session.anubis) != 0)
			topt |= T_NAMES;
		break;

	case KW_RULE_PRIORITY:
		if (strcasecmp(arg, "user") == 0)
			anubis_section_set_prio("RULE", prio_user);
		else if (strcasecmp(arg, "user-only") == 0)
			anubis_section_set_prio("RULE", prio_user_only);
		else if (strcasecmp(arg, "system") == 0)
			anubis_section_set_prio("RULE", prio_system);
		else if (strcasecmp(arg, "system-only") == 0)
			anubis_section_set_prio("RULE", prio_system_only);
		else
			return RC_KW_ERROR;
		break;

	case KW_CONTROL_PRIORITY:
		if (strcasecmp(arg, "user") == 0)
			anubis_section_set_prio("CONTROL", prio_user);
		else if (strcasecmp(arg, "system") == 0)
			anubis_section_set_prio("CONTROL", prio_system);
		else
			return RC_KW_ERROR;
		break;
		
	case KW_TERMLEVEL:           
		if (strcasecmp("silent", arg) == 0)
			options.termlevel = SILENT;
		else if (strcasecmp("normal", arg) == 0)
			options.termlevel = NORMAL;
		else if (strcasecmp("verbose", arg) == 0)
			options.termlevel = VERBOSE;
		else if (strcasecmp("debug", arg) == 0)
			options.termlevel = DEBUG;
		else
			return RC_KW_ERROR;
		break;
		
	case KW_ALLOW_LOCAL_MTA:
		setbool(arg, topt, T_ALLOW_LOCAL_MTA);
		break;
		
	case KW_USER_NOTPRIVILEGED:
		safe_strcpy(session.notprivileged, arg);
		break;

	case KW_LOGFILE:
		if (method == CF_CLIENT) {
			xfree(options.ulogfile);
			options.ulogfile = allocbuf(arg, MAXPATHLEN);
		}
		break;
		
	case KW_LOGLEVEL:
		if (strcasecmp("none", arg) == 0)
			options.uloglevel = NONE;
		else if (strcasecmp("all", arg) == 0)
			options.uloglevel = ALL;
		else if (strcasecmp("fails", arg) == 0)
			options.uloglevel = FAILS;
		else
			return RC_KW_ERROR;
		break;

	case KW_TRACEFILE:
		if (method & (CF_SUPERVISOR|CF_INIT))
			setbool(arg, topt, T_TRACEFILE_SYS);
		else if (method == CF_CLIENT) {
			if (strcasecmp("no", arg) == 0)
				topt &= ~T_TRACEFILE_USR;
			else {
				xfree(options.tracefile);
				if (strcasecmp("yes", arg) == 0) {
					if (options.ulogfile) {
						options.tracefile = strdup(options.ulogfile);
						topt |= T_TRACEFILE_USR;
					}
					else
						topt &= ~T_TRACEFILE_USR;
				}
				else {
					options.tracefile = allocbuf(arg, MAXPATHLEN);
					topt |= T_TRACEFILE_USR;
				}
			}
		}
		break;
	
	case KW_REMOTE_MTA:
		parse_mtaport(arg, session.mta, &session.mta_port);
		break;
		
	case KW_LOCAL_MTA:
		xfree(session.execpath);
		xfree_pptr(session.execargs);
		session.execpath = strdup(arg);
		session.execargs = list_to_argv(arglist);
		topt |= T_LOCAL_MTA;
		break;
		
	case KW_ESMTP_AUTH:
	{
		char *p = strchr(arg, ':');
		if (p) {
			safe_strcpy(session.mta_password, ++p);
			*--p = '\0';
			safe_strcpy(session.mta_username, arg);
			topt |= T_ESMTP_AUTH;
		}
	}		
	break;

	case KW_LOCAL_DOMAIN:
		anubis_domain = strdup(arg);
		break;
		
#ifdef USE_SOCKS_PROXY
	case KW_SOCKS_PROXY:
		parse_mtaport(arg, session.socks, &session.socks_port);
		if_empty_set(session.socks, topt, T_SOCKS);
		break;
		
	case KW_SOCKS_V4:
		setbool(arg, topt, T_SOCKS_V4);
		break;
		
	case KW_SOCKS_AUTH: { 
		char *p = 0;
		p = strchr(arg, ':');
		if (p) {
			safe_strcpy(session.socks_password, ++p);
			*--p = '\0';
			safe_strcpy(session.socks_username, arg);
			topt |= T_SOCKS_AUTH;
		}
		break;
	}
#endif /* USE_SOCKS_PROXY */

	case KW_READ_ENTIRE_BODY:
		setbool(arg, topt, T_ENTIRE_BODY);
		break;

	case KW_DROP_UNKNOWN_USER:
		setbool(arg, topt, T_DROP_UNKNOWN_USER);
		break;

#ifdef WITH_GSASL
	case KW_MODE:
		if (list_count(arglist) != 1) 
			return RC_KW_ERROR;
		if (anubis_set_mode(arg))
			return RC_KW_ERROR;
		break;
		
#endif /* WITH_GSASL */

	default:
		return RC_KW_UNKNOWN;
	}
	return RC_KW_HANDLED;
}

static struct rc_kwdef init_kw[] = {
	{ "bind", KW_BIND },
	{ "local-domain", KW_LOCAL_DOMAIN },
	{ "mode", KW_MODE },
	{ NULL },
};

static struct rc_secdef_child init_sect_child = {
	NULL,
	CF_INIT,
	init_kw,
	control_parser,
	NULL
};

static struct rc_kwdef init_supervisor_kw[] = {
	{ "termlevel",          KW_TERMLEVEL },
	{ "allow-local-mta",    KW_ALLOW_LOCAL_MTA },
	{ "user-notprivileged", KW_USER_NOTPRIVILEGED },
	{ "drop-unknown-user",  KW_DROP_UNKNOWN_USER },
	{ "rule-priority",      KW_RULE_PRIORITY },
	{ "control-priority",   KW_CONTROL_PRIORITY },
	{ NULL }
};

static struct rc_secdef_child init_supervisor_sect_child = {
	NULL,
	CF_INIT|CF_SUPERVISOR,
	init_supervisor_kw,
	control_parser,
	NULL
};

struct rc_kwdef client_kw[] = {
	{ "logfile", KW_LOGFILE },
	{ "loglevel", KW_LOGLEVEL },
	{ NULL },
};

static struct rc_secdef_child client_sect_child = {
	NULL,
	CF_CLIENT,
	client_kw,
	control_parser,
	NULL
};

struct rc_kwdef control_kw[] = {
	{ "remote-mta",  KW_REMOTE_MTA },
	{ "local-mta",   KW_LOCAL_MTA },
	{ "tracefile",   KW_TRACEFILE },
	{ "esmtp-auth",  KW_ESMTP_AUTH, KWF_HIDDEN },
#ifdef USE_SOCKS_PROXY
	{ "socks-proxy", KW_SOCKS_PROXY },
	{ "socks-v4",    KW_SOCKS_V4 },
	{ "socks-auth",  KW_SOCKS_AUTH },
#endif /* USE_SOCKS_PROXY */
	{ "read-entire-body", KW_READ_ENTIRE_BODY },
	{ NULL },
};

static struct rc_secdef_child control_sect_child = {
	NULL,
	CF_ALL,
	control_kw,
	control_parser,
	NULL
};

/* FIXME: This belongs to another file */
#if defined(HAVE_TLS) || defined(HAVE_SSL)
#define KW_SSL                 1
#define KW_SSL_ONEWAY          2 
#define KW_SSL_CERT            3
#define KW_SSL_KEY             4
#define KW_SSL_CAFILE          5

int
tls_parser(int method, int key, LIST *arglist,
	   void *inv_data, void *func_data, MESSAGE *msg)
{
	char *arg = list_item(arglist, 0);
	switch (key) {
	case KW_SSL:
		setbool(arg, topt, T_SSL);
		break;
		
	case KW_SSL_ONEWAY:
		setbool(arg, topt, T_SSL_ONEWAY);
		break;
		
	case KW_SSL_CERT:
		xfree(secure.cert);
		secure.cert = allocbuf(arg, MAXPATHLEN);
		break;
		
	case KW_SSL_KEY:
		xfree(secure.key);
		secure.key = allocbuf(arg, MAXPATHLEN);
		if (method == CF_CLIENT)
			topt |= T_SSL_CKCLIENT;
		break;
		
	case KW_SSL_CAFILE:
		xfree(secure.cafile);
		secure.cafile = allocbuf(arg, MAXPATHLEN);
		break;
		
	default:
		return RC_KW_UNKNOWN;
	}
	return RC_KW_HANDLED;
}

static struct rc_kwdef tls_kw[] = {
	{ "ssl",        KW_SSL },
	{ "ssl-oneway", KW_SSL_ONEWAY },
	{ "ssl-cert",   KW_SSL_CERT },
	{ "ssl-key",    KW_SSL_KEY },
	{ "ssl-cafile", KW_SSL_CAFILE },
	{ NULL }
};

static struct rc_secdef_child tls_sect_child = {
	NULL,
	CF_ALL,
	tls_kw,
	tls_parser,
	NULL
};
#endif /* HAVE_TLS or HAVE_SSL */

void
control_section_init(void)
{
	struct rc_secdef *sp = anubis_add_section("CONTROL");
	sp->prio = prio_system;
	rc_secdef_add_child(sp, &init_sect_child);
	rc_secdef_add_child(sp, &init_supervisor_sect_child);
	rc_secdef_add_child(sp, &client_sect_child);
	rc_secdef_add_child(sp, &control_sect_child);
#if defined(HAVE_TLS) || defined(HAVE_SSL)
	rc_secdef_add_child(sp, &tls_sect_child);
#endif /* HAVE_TLS or HAVE_SSL */
}

/* ************************** The RULE Section *************************** */ 
#define KW_SIGNATURE_FILE_APPEND    1
#define KW_BODY_APPEND              2 
#define KW_BODY_CLEAR_APPEND        3
#define KW_EXTERNAL_BODY_PROCESSOR  4 
#define KW_BODY_CLEAR               5

int
rule_parser(int method, int key, LIST *arglist,
	    void *inv_data, void *func_data, MESSAGE *msg)
{
	char *arg = list_item(arglist, 0);
	char **argv;
	
	switch (key) {
	case KW_SIGNATURE_FILE_APPEND:
		if (strcasecmp("no", arg))
			message_append_signature_file(msg);
		break;
		
	case KW_BODY_APPEND:
		message_append_text_file(msg, arg);
		break;

	case KW_BODY_CLEAR:
		xfree(msg->body);
		msg->body = strdup("");
		break;
		
	case KW_BODY_CLEAR_APPEND:
		xfree(msg->body);
		msg->body = strdup("");
		message_append_text_file(msg, arg);
		break;
		
	case KW_EXTERNAL_BODY_PROCESSOR:
		argv = list_to_argv(arglist);
		message_external_proc(msg, argv);
		xfree_pptr(argv);
		break;
		
	default:
		return RC_KW_UNKNOWN;
	}
	return RC_KW_HANDLED;
}

struct rc_kwdef rule_kw[] = {
	{ "signature-file-append",   KW_SIGNATURE_FILE_APPEND },   
	{ "body-append",	     KW_BODY_APPEND },             
	{ "body-clear-append",	     KW_BODY_CLEAR_APPEND },
	{ "body-clear",              KW_BODY_CLEAR },
	{ "external-body-processor", KW_EXTERNAL_BODY_PROCESSOR },
        { NULL }
};

static struct rc_secdef_child rule_sect_child = {
	NULL,
	CF_CLIENT,
	rule_kw,
	rule_parser,
	NULL
};

void
rule_section_init(void)
{
	struct rc_secdef *sp = anubis_add_section("RULE");
	sp->allow_prog = 1;
	sp->prio = prio_system;
	rc_secdef_add_child(sp, &rule_sect_child);
}

void
rc_system_init(void)
{
	control_section_init();
	translate_section_init();
	rule_section_init();
#ifdef WITH_GUILE
	guile_section_init();
#endif /* WITH_GUILE */
#ifdef HAVE_GPG
	gpg_section_init();
#endif /* HAVE_GPG */
#if defined(WITH_GSASL)
	authmode_section_init();
#endif
}

void
rcfile_process_section(int method, char *name, void *data, MESSAGE *msg)
{
	RC_SECTION *sec;

	for (sec = rc_section_lookup(parse_tree, name);
	     sec;
	     sec = rc_section_lookup(sec->next, name))
		rc_run_section(method, sec, anubis_rc_sections, data, msg);
}

void
rcfile_call_section(int method, char *name, void *data, MESSAGE *msg)
{
	RC_SECTION *sec = rc_section_lookup(parse_tree, name);
	if (!sec)
		anubis_error(SOFT, _("No such section: %s"), name);
	rc_call_section(method, sec, anubis_rc_sections, data, msg);
}

char *
user_rcfile_name()
{
	if (session.rcfile_name)
		return strdup(session.rcfile_name);
	else {
		char homedir[MAXPATHLEN+1];
		char *buf;
		size_t len;
		
		get_homedir(session.clientname, homedir, sizeof(homedir));
		len = strlen(homedir) + 1 + sizeof DEFAULT_LOCAL_RCFILE;
		buf = xmalloc(len);
		strcpy(buf, homedir);
		strcat(buf, "/");
		strcat(buf, DEFAULT_LOCAL_RCFILE);
		return buf;
	}
}

/* EOF */
