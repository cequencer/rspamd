/*
 * Copyright (c) 2009, Rambler media
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Rambler media ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <netdb.h>
#include <math.h>

#include "config.h"
#include "cfg_file.h"
#include "main.h"
#include "filter.h"
#include "classifiers/classifiers.h"
#ifndef HAVE_OWN_QUEUE_H
#include <sys/queue.h>
#else
#include "queue.h"
#endif

#define DEFAULT_SCORE 10.0

extern int yylineno;
extern char *yytext;

int
add_memcached_server (struct config_file *cf, char *str)
{
	char *cur_tok, *err_str;
	struct memcached_server *mc;
	struct hostent *hent;
	uint16_t port;

	if (str == NULL) return 0;

	cur_tok = strsep (&str, ":");

	if (cur_tok == NULL || *cur_tok == '\0') return 0;

	if(cf->memcached_servers_num == MAX_MEMCACHED_SERVERS) {
		yywarn ("yyparse: maximum number of memcached servers is reached %d", MAX_MEMCACHED_SERVERS);
	}
	
	mc = &cf->memcached_servers[cf->memcached_servers_num];
	if (mc == NULL) return 0;
	/* cur_tok - server name, str - server port */
	if (str == NULL) {
		port = DEFAULT_MEMCACHED_PORT;
	}
	else {
		port = (uint16_t)strtoul (str, &err_str, 10);
		if (*err_str != '\0') {
			return 0;
		}
	}

	if (!inet_aton (cur_tok, &mc->addr)) {
		/* Try to call gethostbyname */
		hent = gethostbyname (cur_tok);
		if (hent == NULL) {
			return 0;
		}
		else {
			memcpy((char *)&mc->addr, hent->h_addr, sizeof(struct in_addr));
		}
	}
	mc->port = port;
	cf->memcached_servers_num++;
	return 1;
}

int
parse_bind_line (struct config_file *cf, char *str, char is_control)
{
	char *cur_tok, *err_str;
	struct hostent *hent;
	size_t s;
	
	if (str == NULL) return 0;
	cur_tok = strsep (&str, ":");
	
	if (cur_tok[0] == '/' || cur_tok[0] == '.') {
		if (is_control) {
			cf->control_host = memory_pool_strdup (cf->cfg_pool, cur_tok);
			cf->control_family = AF_UNIX;
		}
		else {
			cf->bind_host = memory_pool_strdup (cf->cfg_pool, cur_tok);
			cf->bind_family = AF_UNIX;
		}
		return 1;

	} else {
		if (str == '\0') {
			if (is_control) {
				cf->control_port = DEFAULT_CONTROL_PORT;
			}
			else {
				cf->bind_port = DEFAULT_BIND_PORT;
			}
		}
		else {
			if (is_control) {
				cf->control_port = (uint16_t)strtoul (str, &err_str, 10);
			}
			else {
				cf->bind_port = (uint16_t)strtoul (str, &err_str, 10);
			}
			if (*err_str != '\0') {
				return 0;
			}
		}
		
		if (is_control) {
			if (!inet_aton (cur_tok, &cf->control_addr)) {
				/* Try to call gethostbyname */
				hent = gethostbyname (cur_tok);
				if (hent == NULL) {
					return 0;
				}
				else {
					cf->control_host = memory_pool_strdup (cf->cfg_pool, cur_tok);
					memcpy((char *)&cf->control_addr, hent->h_addr, sizeof(struct in_addr));
					s = strlen (cur_tok) + 1;
				}
			}
			else {
				cf->control_host = memory_pool_strdup (cf->cfg_pool, cur_tok);
			}

			cf->control_family = AF_INET;
		}
		else {
			if (!inet_aton (cur_tok, &cf->bind_addr)) {
				/* Try to call gethostbyname */
				hent = gethostbyname (cur_tok);
				if (hent == NULL) {
					return 0;
				}
				else {
					cf->bind_host = memory_pool_strdup (cf->cfg_pool, cur_tok);
					memcpy((char *)&cf->bind_addr, hent->h_addr, sizeof(struct in_addr));
					s = strlen (cur_tok) + 1;
				}
			}
			else {
				cf->bind_host = memory_pool_strdup (cf->cfg_pool, cur_tok);
			}

			cf->bind_family = AF_INET;
		}

		return 1;
	}

	return 0;
}

void
init_defaults (struct config_file *cfg)
{
	struct metric *def_metric;

	cfg->memcached_error_time = DEFAULT_UPSTREAM_ERROR_TIME;
	cfg->memcached_dead_time = DEFAULT_UPSTREAM_DEAD_TIME;
	cfg->memcached_maxerrors = DEFAULT_UPSTREAM_MAXERRORS;
	cfg->memcached_protocol = TCP_TEXT;

#ifdef HAVE_SC_NPROCESSORS_ONLN
	cfg->workers_number = sysconf (_SC_NPROCESSORS_ONLN);
#else
	cfg->workers_number = DEFAULT_WORKERS_NUM;
#endif
	cfg->max_statfile_size = DEFAULT_STATFILE_SIZE;
	cfg->modules_opts = g_hash_table_new (g_str_hash, g_str_equal);
	cfg->variables = g_hash_table_new (g_str_hash, g_str_equal);
	cfg->metrics = g_hash_table_new (g_str_hash, g_str_equal);
	cfg->factors = g_hash_table_new (g_str_hash, g_str_equal);
	cfg->c_modules = g_hash_table_new (g_str_hash, g_str_equal);
	cfg->composite_symbols = g_hash_table_new (g_str_hash, g_str_equal);
	cfg->statfiles = g_hash_table_new (g_str_hash, g_str_equal);
	cfg->cfg_params = g_hash_table_new (g_str_hash, g_str_equal);

	def_metric = memory_pool_alloc (cfg->cfg_pool, sizeof (struct metric));
	def_metric->name = "default";
	def_metric->func_name = "factors";
	def_metric->func = factor_consolidation_func;
	def_metric->required_score = DEFAULT_SCORE;
	def_metric->classifier = get_classifier ("winnow");
	g_hash_table_insert (cfg->metrics, "default", def_metric);

	LIST_INIT (&cfg->perl_modules);
}

void
free_config (struct config_file *cfg)
{
	g_hash_table_remove_all (cfg->modules_opts);
	g_hash_table_unref (cfg->modules_opts);
	g_hash_table_remove_all (cfg->variables);
	g_hash_table_unref (cfg->variables);
	g_hash_table_remove_all (cfg->metrics);
	g_hash_table_unref (cfg->metrics);
	g_hash_table_remove_all (cfg->factors);
	g_hash_table_unref (cfg->factors);
	g_hash_table_remove_all (cfg->c_modules);
	g_hash_table_unref (cfg->c_modules);
	g_hash_table_remove_all (cfg->composite_symbols);
	g_hash_table_unref (cfg->composite_symbols);
	g_hash_table_remove_all (cfg->statfiles);
	g_hash_table_unref (cfg->statfiles);
	g_hash_table_remove_all (cfg->cfg_params);
	g_hash_table_unref (cfg->cfg_params);
	memory_pool_delete (cfg->cfg_pool);
}

char* 
get_module_opt (struct config_file *cfg, char *module_name, char *opt_name)
{
	LIST_HEAD (moduleoptq, module_opt) *cur_module_opt = NULL;
	struct module_opt *cur;
	
	cur_module_opt = g_hash_table_lookup (cfg->modules_opts, module_name);
	if (cur_module_opt == NULL) {
		return NULL;
	}

	LIST_FOREACH (cur, cur_module_opt, next) {
		if (strcmp (cur->param, opt_name) == 0) {
			return cur->value;
		}
	}

	return NULL;
}

size_t
parse_limit (const char *limit)
{
	size_t result = 0;
	char *err_str;

	if (!limit || *limit == '\0') return 0;

	result = strtoul (limit, &err_str, 10);

	if (*err_str != '\0') {
		/* Megabytes */
		if (*err_str == 'm' || *err_str == 'M') {
			result *= 1048576L;
		}
		/* Kilobytes */
		else if (*err_str == 'k' || *err_str == 'K') {
			result *= 1024;
		}
		/* Gigabytes */
		else if (*err_str == 'g' || *err_str == 'G') {
			result *= 1073741824L;
		}
	}

	return result;
}

unsigned int
parse_seconds (const char *t)
{
	unsigned int result = 0;
	char *err_str;

	if (!t || *t == '\0') return 0;

	result = strtoul (t, &err_str, 10);

	if (*err_str != '\0') {
		/* Seconds */
		if (*err_str == 's' || *err_str == 'S') {
			result *= 1000;
		}
	}

	return result;
}

char
parse_flag (const char *str)
{
	if (!str || !*str) return -1;

	if ((*str == 'Y' || *str == 'y') && *(str + 1) == '\0') {
		return 1;
	}

	if ((*str == 'Y' || *str == 'y') &&
		(*(str + 1) == 'E' || *(str + 1) == 'e') &&
		(*(str + 2) == 'S' || *(str + 2) == 's') &&
		*(str + 3) == '\0') {
		return 1;		
	}

	if ((*str == 'N' || *str == 'n') && *(str + 1) == '\0') {
		return 0;
	}

	if ((*str == 'N' || *str == 'n') &&
		(*(str + 1) == 'O' || *(str + 1) == 'o') &&
		*(str + 2) == '\0') {
		return 0;		
	}

	return -1;
}

/*
 * Try to substitute all variables in given string
 * Return: newly allocated string with substituted variables (original string may be freed if variables are found)
 */
char *
substitute_variable (struct config_file *cfg, char *str, u_char recursive)
{
	char *var, *new, *v_begin, *v_end;
	size_t len;

	while ((v_begin = strstr (str, "${")) != NULL) {
		len = strlen (str);
		*v_begin = '\0';
		v_begin += 2;
		if ((v_end = strstr (v_begin, "}")) == NULL) {
			/* Not a variable, skip */
			continue;
		}
		*v_end = '\0';
		var = g_hash_table_lookup (cfg->variables, v_begin);
		if (var == NULL) {
			yywarn ("substitute_variable: variable %s is not defined", v_begin);
			/* Substitute unknown variables with empty string */
			var = "";
		}
		else if (recursive) {
			var = substitute_variable (cfg, var, recursive);
		}
		/* Allocate new string */
		new = memory_pool_alloc (cfg->cfg_pool, len - strlen (v_begin) + strlen (var) + 1);

		snprintf (new, len - strlen (v_begin) + strlen (var) + 1, "%s%s%s",
						str, var, v_end + 1);
		str = new;
	}

	return str;
}

static void
substitute_module_variables (gpointer key, gpointer value, gpointer data)
{
	struct config_file *cfg = (struct config_file *)data;
	LIST_HEAD (moduleoptq, module_opt) *cur_module_opt = (struct moduleoptq *)value;
	struct module_opt *cur, *tmp;

	LIST_FOREACH_SAFE (cur, cur_module_opt, next, tmp) {
		if (cur->value) {
			cur->value = substitute_variable (cfg, cur->value, 0);
		}
	}
}

static void
substitute_all_variables (gpointer key, gpointer value, gpointer data)
{
	struct config_file *cfg = (struct config_file *)data;
	char *var;

	var = value;
	/* Do recursive substitution */
	var = substitute_variable (cfg, var, 1);
}

static void
parse_filters_str (struct config_file *cfg, const char *str, enum script_type type)
{
	gchar **strvec, **p;
	struct filter *cur;
	int i;
	
	if (str == NULL) {
		return;	
	}

	strvec = g_strsplit (str, ",", 0);
	if (strvec == NULL) {
		return;
	}

	p = strvec;
	while (*p) {
		cur = NULL;
		/* Search modules from known C modules */
		for (i = 0; i < MODULES_NUM; i++) {
			if (strcasecmp (modules[i].name, *p) == 0) {
				cur = memory_pool_alloc (cfg->cfg_pool, sizeof (struct filter));
				cur->type = C_FILTER;
				msg_debug ("parse_filters_str: found C filter %s", *p);
				switch (type) {
					case SCRIPT_HEADER:
						cur->func_name = memory_pool_strdup (cfg->cfg_pool, *p);
						LIST_INSERT_HEAD (&cfg->header_filters, cur, next);
						break;
					case SCRIPT_MIME:
						cur->func_name = memory_pool_strdup (cfg->cfg_pool, *p);
						LIST_INSERT_HEAD (&cfg->mime_filters, cur, next);
						break;
					case SCRIPT_MESSAGE:
						cur->func_name = memory_pool_strdup (cfg->cfg_pool, *p);
						LIST_INSERT_HEAD (&cfg->message_filters, cur, next);
						break;
					case SCRIPT_URL:
						cur->func_name = memory_pool_strdup (cfg->cfg_pool, *p);
						LIST_INSERT_HEAD (&cfg->url_filters, cur, next);
						break;
				}
				break;
			}	
		}
		if (cur != NULL) {
			/* Go to next iteration */
			p++;
			continue;
		}
		cur = memory_pool_alloc (cfg->cfg_pool, sizeof (struct filter));
		cur->type = PERL_FILTER;
		switch (type) {
			case SCRIPT_HEADER:
				cur->func_name = memory_pool_strdup (cfg->cfg_pool, *p);
				LIST_INSERT_HEAD (&cfg->header_filters, cur, next);
				break;
			case SCRIPT_MIME:
				cur->func_name = memory_pool_strdup (cfg->cfg_pool, *p);
				LIST_INSERT_HEAD (&cfg->mime_filters, cur, next);
				break;
			case SCRIPT_MESSAGE:
				cur->func_name = memory_pool_strdup (cfg->cfg_pool, *p);
				LIST_INSERT_HEAD (&cfg->message_filters, cur, next);
				break;
			case SCRIPT_URL:
				cur->func_name = memory_pool_strdup (cfg->cfg_pool, *p);
				LIST_INSERT_HEAD (&cfg->url_filters, cur, next);
				break;
		}
		p ++;
	}

	g_strfreev (strvec);
}

/*
 * Place pointers to cfg_file structure to hash cfg_params
 */
static void
fill_cfg_params (struct config_file *cfg)
{
    struct config_scalar *scalars;

    scalars = memory_pool_alloc (cfg->cfg_pool, 10 * sizeof (struct config_scalar));

    scalars[0].type = SCALAR_TYPE_STR;
    scalars[0].pointer = &cfg->cfg_name;
    g_hash_table_insert (cfg->cfg_params, "cfg_name", &scalars[0]);
    scalars[1].type = SCALAR_TYPE_STR;
    scalars[1].pointer = &cfg->pid_file;
    g_hash_table_insert (cfg->cfg_params, "pid_file", &scalars[1]);
    scalars[2].type = SCALAR_TYPE_STR;
    scalars[2].pointer = &cfg->temp_dir;
    g_hash_table_insert (cfg->cfg_params, "temp_dir", &scalars[2]);
    scalars[3].type = SCALAR_TYPE_STR;
    scalars[3].pointer = &cfg->bind_host;
    g_hash_table_insert (cfg->cfg_params, "bind_host", &scalars[3]);
    scalars[4].type = SCALAR_TYPE_STR;
    scalars[4].pointer = &cfg->control_host;
    g_hash_table_insert (cfg->cfg_params, "control_host", &scalars[4]);
    scalars[5].type = SCALAR_TYPE_INT;
    scalars[5].pointer = &cfg->controller_enabled;
    g_hash_table_insert (cfg->cfg_params, "controller_enabled", &scalars[5]);
    scalars[6].type = SCALAR_TYPE_STR;
    scalars[6].pointer = &cfg->control_password;
    g_hash_table_insert (cfg->cfg_params, "control_password", &scalars[6]);
    scalars[7].type = SCALAR_TYPE_INT;
    scalars[7].pointer = &cfg->no_fork;
    g_hash_table_insert (cfg->cfg_params, "no_fork", &scalars[7]);
    scalars[8].type = SCALAR_TYPE_UINT;
    scalars[8].pointer = &cfg->workers_number;
    g_hash_table_insert (cfg->cfg_params, "workers_number", &scalars[8]);
    scalars[9].type = SCALAR_TYPE_SIZE;
    scalars[9].pointer = &cfg->max_statfile_size;
    g_hash_table_insert (cfg->cfg_params, "max_statfile_size", &scalars[9]);

}

/* 
 * Perform post load actions
 */
void
post_load_config (struct config_file *cfg)
{
	g_hash_table_foreach (cfg->variables, substitute_all_variables, cfg);
	g_hash_table_foreach (cfg->modules_opts, substitute_module_variables, cfg);
	parse_filters_str (cfg, cfg->header_filters_str, SCRIPT_HEADER);
	parse_filters_str (cfg, cfg->mime_filters_str, SCRIPT_MIME);
	parse_filters_str (cfg, cfg->message_filters_str, SCRIPT_MESSAGE);
	parse_filters_str (cfg, cfg->url_filters_str, SCRIPT_URL);
    fill_cfg_params (cfg);
}

/*
 * Rspamd regexp utility functions
 */
struct rspamd_regexp*
parse_regexp (memory_pool_t *pool, char *line)
{
	char *begin, *end, *p;
	struct rspamd_regexp *result;
	int regexp_flags = 0;
	enum rspamd_regexp_type type = REGEXP_NONE;
	GError *err = NULL;
	
	result = memory_pool_alloc0 (pool, sizeof (struct rspamd_regexp));
	/* First try to find header name */
	begin = strchr (line, '=');
	if (begin != NULL) {
		*begin = '\0';
		result->header = memory_pool_strdup (pool, line);
		result->type = REGEXP_HEADER;
		*begin = '=';
		line = begin;
	}
	/* Find begin of regexp */
	while (*line != '/') {
		line ++;
	}
	if (*line != '\0') {
		begin = line + 1;
	}
	else if (result->header == NULL) {
		/* Assume that line without // is just a header name */
		result->header = memory_pool_strdup (pool, line);
		result->type = REGEXP_HEADER;
		return result;
	}
	else {
		/* We got header name earlier but have not found // expression, so it is invalid regexp */
		return NULL;
	}
	/* Find end */
	end = begin;
	while (*end && (*end != '/' || *(end - 1) == '\\')) {
		end ++;
	}
	if (end == begin || *end != '/') {
		return NULL;
	}
	/* Parse flags */
	p = end + 1;
	while (p != NULL) {
		switch (*p) {
			case 'i':
				regexp_flags |= G_REGEX_CASELESS;
				p ++;
				break;
			case 'm':
				regexp_flags |= G_REGEX_MULTILINE;
				p ++;
				break;
			case 's':
				regexp_flags |= G_REGEX_DOTALL;
				p ++;
				break;
			case 'x':
				regexp_flags |= G_REGEX_EXTENDED;
				p ++;
				break;
			case 'u':
				regexp_flags |= G_REGEX_UNGREEDY;
				p ++;
				break;
			case 'o':
				regexp_flags |= G_REGEX_OPTIMIZE;
				p ++;
				break;
			/* Type flags */
			case 'H':
				if (type != REGEXP_NONE) {
					type = REGEXP_HEADER;
				}
				p ++;
				break;
			case 'M':
				if (type != REGEXP_NONE) {
					type = REGEXP_MESSAGE;
				}
				p ++;
				break;
			case 'P':
				if (type != REGEXP_NONE) {
					type = REGEXP_MIME;
				}
				p ++;
				break;
			case 'U':
				if (type != REGEXP_NONE) {
					type = REGEXP_URL;
				}
				p ++;
				break;
			/* Stop flags parsing */
			default:
				p = NULL;
				break;
		}
	}

	result = memory_pool_alloc (pool, sizeof (struct rspamd_regexp));
	result->type = type;
	*end = '\0';
	result->regexp = g_regex_new (begin, regexp_flags, 0, &err);
	result->regexp_text = memory_pool_strdup (pool, begin);
	memory_pool_add_destructor (pool, (pool_destruct_func)g_regex_unref, (void *)result->regexp);
	*end = '/';

	return result;
}

void
parse_err (const char *fmt, ...)
{
	va_list aq;
	char logbuf[BUFSIZ], readbuf[32];
	int r;
	
	va_start (aq, fmt);
	g_strlcpy (readbuf, yytext, sizeof (readbuf));

	r = snprintf (logbuf, sizeof (logbuf), "config file parse error! line: %d, text: %s, reason: ", yylineno, readbuf);
	r += vsnprintf (logbuf + r, sizeof (logbuf) - r, fmt, aq);

	va_end (aq);
	g_error ("%s", logbuf);
}

void
parse_warn (const char *fmt, ...)
{
	va_list aq;
	char logbuf[BUFSIZ], readbuf[32];
	int r;
	
	va_start (aq, fmt);
	g_strlcpy (readbuf, yytext, sizeof (readbuf));

	r = snprintf (logbuf, sizeof (logbuf), "config file parse warning! line: %d, text: %s, reason: ", yylineno, readbuf);
	r += vsnprintf (logbuf + r, sizeof (logbuf) - r, fmt, aq);

	va_end (aq);
	g_warning ("%s", logbuf);
}


/*
 * vi:ts=4
 */
