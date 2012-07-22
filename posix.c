/*
 *  GT.M POSIX Extension
 *  Copyright (C) 2012 Piotr Koper <piotr.koper@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


/*
 * GT.M POSIX Extension
 *
 *
 * RETURN VALUE & ERRNO
 *
 * This extensions tries to convert standard POSIX C way of returning status
 * code and errno setup into a well known programming practise for M, where
 * error condition raises an exception.
 *
 * The above statement implies that wrapper functions for POSIX should:
 * 1. return errno when POSIX function return code is:
 *    a) not intended to be used in M code, for e.g. some type of
 *       ssize_t meaning bytes written into a buffer, or -1 for a failure,
 *       or return value is really useless, for e.g. times(2) where "return
 *       value may overflow the possible range", overlaps with error
 *       code -1 and has different meaning between implementations,
 *    b) "0 for success, or -1 for failure (in which case errno is set
 *        appropriately)",
 *    c) "a pointer to a [...], or NULL if an error occurs",
 * 2. return POSIX function return value when error condition should never
 *    occur, for e.g. time(2) can produce an error only when passed pointer
 *    "points outside your accessible address space" which would be 
 *    programming error on the level of this C library or an error in GT.M.
 * 3. return void when there is nothing to return,
 * 4. return POSIX function return value when there is no errno, this is the
 *    only case, where error code should leak into M world.
 *
 * Cases:
 * 1.a)
 *    readlink, strftime, times (useless return value)
 * 1.b)
 *    clock_gettime, clock_getres, sysinfo, uname, setenv, unsetenv, stat,
 *    stat, link, symlink, unlink, mkdir, rmdir, chmod, chown, lchown
 * 1.c)
 *    localtime, gmtime, getpwnam, getpwuid, getgrnam, getgrgid
 * 2.
 *    time, umask
 * 3.
 *    openlog, syslog
 * 4.
 *    mktime
 *
 * NOTICE: "%SYSTEM-E-ENO61, No data available" error code is used to
 * signalize the user that the function has been called with invalid number
 * of arguments, for e.g. clock_gettime(3) call which expects 3 arguments:
 *   set errno=$&posix.clockgettime()
 * raises an exception and sets errno to 61 (ENODATA).
 *
 *
 * STRINGIFIED OPTION NAMES
 * 
 * This extension exposes stringified POSIX options names instead of
 * passing an integer, as an effort to deliver look and feel of M calling
 * convention. This way there is no need for unnatural determining the
 * value for integer option from C header files. The stringified option
 * name has stripped common prefix.
 *
 *   Example M code for clock_gettime(2) POSIX function call:
 *     set errno=$&posix.clockgettime("REALTIME",.sec,.nsec)
 *
 * Multiple values for an option are passed as single string with names
 * separated with "|" char.
 *
 *   Example M code for openlog(2) POSIX function call:
 *     d &posix.openlog("program1","NDELAY|PID","USER")
 *
 *
 * FILE MODE
 *
 * The exception from passing stringified option names are file permissions.
 * Octal values for file mode are better known than S_I* flags (e.g. S_IRUSR),
 * so instead of providing stringified version of flag names, conversion from
 * octal value to integer are performed in M code, so that M functions
 * support (only) octal mode, for e.g.:
 *   d mkdir^posix("/tmp/t1",0755)
 *
 */

#include <errno.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/times.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include "gtmxc_types.h"


#define UNUSED __attribute__ ((unused))

/* strlen (list_delimiter) == 1 */
#define list_delimiter "|"

typedef struct
{
	char *name;
	int value;
}
param;

#define check_argc(x) do { \
		if (argc != x) \
			return ENODATA; \
	} while (0)

#define check(x) do { \
		if ((x) != 0) \
			return EINVAL; \
	} while (0);

#define clear_errno() (errno = 0)

#define get_param(x) _get_param (x##_param, sizeof (x##_param), x##_name, &x)

static int
_get_param (param *p, size_t s, char *name, int *value)
{
	unsigned int i;
	for (i = 0; i < s / sizeof (param); i++)
		if (strcasecmp (p[i].name, name) == 0)
		{
			*value = p[i].value;
			return 0;
		}
	return -1;
}

#define get_flags(x) _get_flags (x##_param, sizeof (x##_param), x##_name, &x)
static int
_get_flags (param *param, size_t s, char *name, int *value)
{
	int flags = 0;
	int f;
	char *p = strtok (name, list_delimiter);
	while (p != NULL)
	{
		check (_get_param (param, s, p, &f));
		flags |= f;
		p = strtok (NULL, list_delimiter);
	}
	*value = flags;
	return 0;
}

int
strncopy (char *p, char *s, ssize_t n)
{
	while (n > 1)
	{
		if ((*p = *s) == '\0')
			return 0;
		p++;
		s++;
		n--;
	}
	*p = '\0';
	return ERANGE;
}

char *
strpncopy (char *p, char *s, ssize_t *n)
{
	while (*n > 1)
	{
		if ((*p = *s) == '\0')
			return p;
		p++;
		s++;
		(*n)--;
	}
	*p = '\0';
	return NULL;
}

#define push_list(x, list, count) list[count++] = x

#define check_list(count, limit) \
	do { \
		if (count >= limit) \
			return EMFILE; \
	} while (0)

static int
in_list (void *p, void **list, int *count, int remove)
{
	int i;
	for (i = 0; i < *count; i++)
		if (list[i] == p)
		{
			if (remove)
				list[i] = list[--(*count)];
			return 1;
		}
	return 0;
}

gtm_ulong_t
posix_time (int argc UNUSED)
{
	return time (NULL);
}

static gtm_status_t
_posix_clock_get (int argc, gtm_char_t *clk_id_name, gtm_long_t *tv_sec, gtm_long_t *tv_nsec, int gettime)
{
	struct timespec b;
	int clk_id;
	param clk_id_param[] = {
		{ "REALTIME",		CLOCK_REALTIME },
		{ "MONOTONIC",		CLOCK_MONOTONIC },
		{ "MONOTONIC_RAW",	CLOCK_MONOTONIC_RAW },
		{ "PROCESS_CPUTIME_ID",	CLOCK_PROCESS_CPUTIME_ID },
		{ "THREAD_CPUTIME_ID",	CLOCK_THREAD_CPUTIME_ID },
	};
	check_argc (3);
	check (get_param (clk_id));
	memset (&b, '\0', sizeof (b));
	clear_errno ();
	gettime ? clock_gettime (clk_id, &b) : clock_getres (clk_id, &b);
	*tv_sec = b.tv_sec;
	*tv_nsec = b.tv_nsec;
	return errno;
}

gtm_status_t
posix_clock_gettime (int argc, gtm_char_t *clk_id_name, gtm_long_t *tv_sec, gtm_long_t *tv_nsec)
{
	return _posix_clock_get (argc, clk_id_name, tv_sec, tv_nsec, 1);
}

gtm_status_t
posix_clock_getres (int argc, gtm_char_t *clk_id_name, gtm_long_t *tv_sec, gtm_long_t *tv_nsec)
{
	return _posix_clock_get (argc, clk_id_name, tv_sec, tv_nsec, 0);
}

static gtm_status_t
_posix_time (int argc, gtm_long_t* t,
	gtm_int_t *tm_sec, gtm_int_t *tm_min, gtm_int_t *tm_hour, gtm_int_t *tm_mday, gtm_int_t *tm_mon,
	gtm_int_t *tm_year, gtm_int_t *tm_wday, gtm_int_t *tm_yday, gtm_int_t *tm_isdst,
	int local)
{
	struct tm *b;
	check_argc (10);
	clear_errno ();
	if ((b = local ? localtime (t) : gmtime (t)) != NULL)
	{
		*tm_sec = b -> tm_sec;
		*tm_min = b -> tm_min;
		*tm_hour = b -> tm_hour;
		*tm_mday = b -> tm_mday;
		*tm_mon = b -> tm_mon;
		*tm_year = b -> tm_year;
		*tm_wday = b -> tm_wday;
		*tm_yday = b -> tm_yday;
		*tm_isdst = b -> tm_isdst;
	}
	else
	{
		*tm_sec = 0;
		*tm_min = 0;
		*tm_hour = 0;
		*tm_mday = 0;
		*tm_mon = 0;
		*tm_year = 0;
		*tm_wday = 0;
		*tm_yday = 0;
		*tm_isdst = 0;
	}
	return errno;
}

gtm_status_t
posix_localtime (int argc, gtm_long_t* t,
	gtm_int_t *tm_sec, gtm_int_t *tm_min, gtm_int_t *tm_hour, gtm_int_t *tm_mday, gtm_int_t *tm_mon,
	gtm_int_t *tm_year, gtm_int_t *tm_wday, gtm_int_t *tm_yday, gtm_int_t *tm_isdst)
{
	return _posix_time (argc, t, tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst, 1);
}

gtm_status_t
posix_gmtime (int argc, gtm_long_t* t,
	gtm_int_t *tm_sec, gtm_int_t *tm_min, gtm_int_t *tm_hour, gtm_int_t *tm_mday, gtm_int_t *tm_mon,
	gtm_int_t *tm_year, gtm_int_t *tm_wday, gtm_int_t *tm_yday, gtm_int_t *tm_isdst)
{
	return _posix_time (argc, t, tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst, 0);
}

gtm_long_t
posix_mktime (int argc, 
	gtm_int_t *tm_sec, gtm_int_t *tm_min, gtm_int_t *tm_hour, gtm_int_t *tm_mday, gtm_int_t *tm_mon,
	gtm_int_t *tm_year, gtm_int_t *tm_wday, gtm_int_t *tm_yday, gtm_int_t *tm_isdst)
{
	struct tm b;
	check_argc (9);
	b.tm_sec = *tm_sec;
	b.tm_min = *tm_min;
	b.tm_hour = *tm_hour;
	b.tm_mday = *tm_mday;
	b.tm_mon = *tm_mon;
	b.tm_year = *tm_year;
	b.tm_wday = *tm_wday;
	b.tm_yday = *tm_yday;
	b.tm_isdst = *tm_isdst;
	return mktime (&b);
}

gtm_status_t
posix_strftime (int argc, 
	gtm_char_t* format,
	gtm_int_t *tm_sec, gtm_int_t *tm_min, gtm_int_t *tm_hour, gtm_int_t *tm_mday, gtm_int_t *tm_mon,
	gtm_int_t *tm_year, gtm_int_t *tm_wday, gtm_int_t *tm_yday, gtm_int_t *tm_isdst,
	gtm_char_t *s /* [128] */)
{
	struct tm b;
	check_argc (11);
	b.tm_sec = *tm_sec;
	b.tm_min = *tm_min;
	b.tm_hour = *tm_hour;
	b.tm_mday = *tm_mday;
	b.tm_mon = *tm_mon;
	b.tm_year = *tm_year;
	b.tm_wday = *tm_wday;
	b.tm_yday = *tm_yday;
	b.tm_isdst = *tm_isdst;
	s[0] = '\0';
	clear_errno ();
	strftime (s, 127, format, &b);
	s[127] = '\0';
	return errno;
}

gtm_status_t
posix_times(int argc,
	gtm_long_t *tms_utime, gtm_long_t *tms_stime, gtm_long_t *tms_cutime, gtm_long_t *tms_cstime)
{
	struct tms b;
	check_argc (4);
	memset (&b, '\0', sizeof (b));
	clear_errno ();
	times (&b);
	*tms_utime = b.tms_utime;
	*tms_stime = b.tms_stime;
	*tms_cutime = b.tms_cutime;
	*tms_cstime = b.tms_cstime;
	return errno;
}

gtm_status_t
posix_sysinfo(int argc,
	gtm_long_t *uptime,
	gtm_ulong_t *load1, gtm_ulong_t *load5, gtm_ulong_t *load15,
	gtm_ulong_t *totalram, gtm_ulong_t *freeram, gtm_ulong_t *sharedram,
	gtm_ulong_t *bufferram, gtm_ulong_t *totalswap, gtm_ulong_t *freeswap,
	gtm_uint_t *procs,
	gtm_ulong_t *totalhigh, gtm_ulong_t *freehigh,
	gtm_uint_t *mem_unit)
{
	struct sysinfo b;
	check_argc (14);
	memset (&b, '\0', sizeof (b));
	clear_errno ();
	if (sysinfo (&b) == 0)
	{
		*uptime = b.uptime;
		*load1 = b.loads[0];
		*load5 = b.loads[1];
		*load15 = b.loads[2];
		*totalram = b.totalram;
		*freeram = b.freeram;
		*sharedram = b.sharedram;
		*bufferram = b.bufferram;
		*totalswap = b.totalswap;
		*freeswap = b.freeswap;
		*procs = b.procs;
		*totalhigh = b.totalhigh;
		*freehigh = b.freehigh;
		*mem_unit = b.mem_unit;
	}
	return errno;
}


gtm_status_t
posix_uname (int argc,
	gtm_char_t *sysname /* [128] */, gtm_char_t *nodename /* [128] */,
	gtm_char_t *release /* [128] */, gtm_char_t *version /* [128] */,
	gtm_char_t *machine /* [128] */)
{
	struct utsname b;
	check_argc (5);
	clear_errno ();
	sysname[0] = '\0';
	nodename[0] = '\0';
	release[0] = '\0';
	version[0] = '\0';
	machine[0] = '\0';
	if (uname (&b) == 0)
	{
		if (strncopy (sysname, b.sysname, 128) ||
			strncopy (nodename, b.nodename, 128) ||
			strncopy (release, b.release, 128) ||
			strncopy (version, b.version, 128) ||
			strncopy (machine, b.machine, 128))
				return ERANGE;
	}
	return errno;
}

gtm_status_t
posix_setenv (int argc, gtm_char_t *name, gtm_char_t *value, gtm_int_t overwrite)
{
	check_argc (3);
	setenv (name, value, overwrite);
	return errno;
}

gtm_status_t
posix_unsetenv (int argc, gtm_char_t *name)
{
	check_argc (1);
	unsetenv (name);
	return errno;
}

gtm_status_t
posix_openlog (int argc, gtm_char_t *ident, gtm_char_t *option_name, gtm_char_t *facility_name)
{
	int option;
	int facility;
	char *p;
	size_t s;
	param option_param[] = {
		{ "CONS",	LOG_CONS },
		{ "NDELAY",	LOG_NDELAY },
		{ "NOWAIT",	LOG_NOWAIT },
		{ "PID",	LOG_PID }
	};
	param facility_param[] = {
		{ "AUTH",	LOG_AUTH },
		{ "AUTHPRIV",	LOG_AUTHPRIV },
		{ "CRON",	LOG_CRON },
		{ "DAEMON",	LOG_DAEMON },
		{ "FTP",	LOG_FTP },
		{ "KERN",	LOG_KERN },
		{ "LOCAL0",	LOG_LOCAL0 },
		{ "LOCAL1",	LOG_LOCAL1 },
		{ "LOCAL2",	LOG_LOCAL2 },
		{ "LOCAL3",	LOG_LOCAL3 },
		{ "LOCAL4",	LOG_LOCAL4 },
		{ "LOCAL5",	LOG_LOCAL5 },
		{ "LOCAL6",	LOG_LOCAL6 },
		{ "LOCAL7",	LOG_LOCAL7 },
		{ "LPR",	LOG_LPR },
		{ "MAIL",	LOG_MAIL },
		{ "NEWS",	LOG_NEWS },
		{ "SYSLOG",	LOG_SYSLOG },
		{ "USER",	LOG_USER },
		{ "UUCP",	LOG_UUCP }
	};
	check_argc (3);
	s = strlen (ident) + 1;
	clear_errno ();
	/*
	 * "The argument ident in the call of openlog() is probably
	 * stored as-is.  Thus, if the string it points to is changed,
	 * syslog() may start prepending the changed string, and if
	 * the string it points to ceases to exist, the results are
	 * undefined."
	 */
	if ((p = malloc (s)) == NULL)
		return errno;
	memcpy (p, ident, s);
	check (get_flags (option));
	check (get_flags (facility));
	openlog (p, option, facility);
	return errno;
}

gtm_status_t
posix_syslog (int argc, gtm_char_t *priority_name, gtm_char_t *message)
{
	int priority;
	param priority_param[] = {
		{ "EMERG",	LOG_EMERG },
		{ "ALERT",	LOG_ALERT },
		{ "CRIT",	LOG_CRIT },
		{ "ERR",	LOG_ERR },
		{ "WARNING",	LOG_WARNING },
		{ "NOTICE",	LOG_NOTICE },
		{ "INFO",	LOG_INFO },
		{ "DEBUG",	LOG_DEBUG },
	};
	check_argc (2);
	check (get_param (priority));
	clear_errno ();
	syslog (priority, "%s", message);
	return errno;
}

gtm_ulong_t
posix_umask (int argc, gtm_ulong_t mask)
{
	check_argc (1);
	return umask (mask);
}

static gtm_status_t
_posix_stat (int argc,
	gtm_char_t *path,
	gtm_ulong_t *st_dev, gtm_ulong_t *st_ino, gtm_ulong_t *st_mode, gtm_ulong_t *st_nlink,
	gtm_ulong_t *st_uid, gtm_ulong_t *st_gid, gtm_ulong_t *st_rdev, gtm_ulong_t *st_size,
	gtm_ulong_t *st_blksize, gtm_ulong_t *st_blocks, gtm_ulong_t *atime, gtm_ulong_t *mtime,
	gtm_ulong_t *ctime,
	int l)
{
	struct stat b;
	check_argc (14);
	memset (&b, '\0', sizeof (b));
	clear_errno ();
	if ((l ? lstat (path, &b) : stat (path, &b)) == 0)
	{
		*st_dev = b.st_dev;
		*st_ino = b.st_ino;
		*st_mode = b.st_mode;
		*st_nlink = b.st_nlink;
		*st_uid = b.st_uid;
		*st_gid = b.st_gid;
		*st_rdev = b.st_rdev;
		*st_size = b.st_size;
		*st_blksize = b.st_blksize;
		*st_blocks = b.st_blocks;
		*atime = b.st_atime;
		*mtime = b.st_mtime;
		*ctime = b.st_ctime;
	}
	return errno;
}

gtm_status_t
posix_stat (int argc,
	gtm_char_t *path,
	gtm_ulong_t *st_dev, gtm_ulong_t *st_ino, gtm_ulong_t *st_mode, gtm_ulong_t *st_nlink,
	gtm_ulong_t *st_uid, gtm_ulong_t *st_gid, gtm_ulong_t *st_rdev, gtm_ulong_t *st_size,
	gtm_ulong_t *st_blksize, gtm_ulong_t *st_blocks, gtm_ulong_t *atime, gtm_ulong_t *mtime,
	gtm_ulong_t *ctime)
{
	return _posix_stat (argc, path, st_dev, st_ino, st_mode, st_nlink, st_uid, st_gid, st_rdev,
		st_size, st_blksize, st_blocks, atime, mtime, ctime, 0);
}

gtm_status_t
posix_lstat (int argc,
	gtm_char_t *path,
	gtm_ulong_t *st_dev, gtm_ulong_t *st_ino, gtm_ulong_t *st_mode, gtm_ulong_t *st_nlink,
	gtm_ulong_t *st_uid, gtm_ulong_t *st_gid, gtm_ulong_t *st_rdev, gtm_ulong_t *st_size,
	gtm_ulong_t *st_blksize, gtm_ulong_t *st_blocks, gtm_ulong_t *atime, gtm_ulong_t *mtime,
	gtm_ulong_t *ctime)
{
	return _posix_stat (argc, path, st_dev, st_ino, st_mode, st_nlink, st_uid, st_gid, st_rdev,
		st_size, st_blksize, st_blocks, atime, mtime, ctime, 1);
}

gtm_status_t
posix_readlink (int argc, gtm_char_t* path, gtm_char_t* name /* [1024] */)
{
	ssize_t s;
	check_argc (2);
	clear_errno ();
	s = readlink (path, name, 1023);
	name[s] = '\0';
	return errno;
}

static gtm_status_t
_posix_link (int argc, gtm_char_t *oldpath, gtm_char_t *newpath, int sym)
{
	check_argc (2);
	clear_errno ();
	sym ? symlink (oldpath, newpath) : link (oldpath, newpath);
	return errno;
}

gtm_status_t
posix_link (int argc, gtm_char_t *oldpath, gtm_char_t *newpath)
{
	return _posix_link (argc, oldpath, newpath, 0);
}

gtm_status_t
posix_symlink (int argc, gtm_char_t *oldpath, gtm_char_t *newpath)
{
	return _posix_link (argc, oldpath, newpath, 1);
}

gtm_status_t
posix_unlink (int argc, gtm_char_t *path)
{
	check_argc (1);
	clear_errno ();
	unlink (path);
	return errno;
}

gtm_status_t
posix_mkdir (int argc, gtm_char_t *path, gtm_ulong_t mode)
{
	check_argc (2);
	clear_errno ();
	mkdir (path, mode);
	return errno;
}

gtm_status_t
posix_rmdir (int argc, gtm_char_t *path)
{
	check_argc (1);
	clear_errno ();
	rmdir (path);
	return errno;
}

gtm_status_t
posix_chmod (int argc, gtm_char_t *path, gtm_ulong_t mode)
{
	check_argc (2);
	clear_errno ();
	chmod (path, mode);
	return errno;
}

static gtm_status_t
_posix_chown (int argc, gtm_char_t *path, gtm_ulong_t uid, gtm_ulong_t gid, int l)
{
	check_argc (3);
	clear_errno ();
	l ? lchown (path, uid, gid) : chown (path, uid, gid);
	return errno;
}

gtm_status_t
posix_chown (int argc, gtm_char_t *path, gtm_ulong_t uid, gtm_ulong_t gid)
{
	return _posix_chown (argc, path, uid, gid, 0);
}

gtm_status_t
posix_lchown (int argc, gtm_char_t *path, gtm_ulong_t uid, gtm_ulong_t gid)
{
	return _posix_chown (argc, path, uid, gid, 1);
}

static gtm_status_t
_posix_getpw (int argc,
	gtm_char_t *name,
	gtm_ulong_t uid,
	gtm_char_t *pw_name /* [64] */, gtm_char_t *pw_passwd /* [64] */,
	gtm_ulong_t *pw_uid, gtm_ulong_t *pw_gid,
	gtm_char_t *pw_gecos /* [256] */, gtm_char_t *pw_dir /* [1024] */, gtm_char_t *pw_shell /* [1024] */)
{
	struct passwd *b;
	check_argc (8);
	*pw_uid = 0;
	*pw_gid = 0;
	pw_name[0] = '\0';
	pw_passwd[0] = '\0';
	pw_gecos[0] = '\0';
	pw_dir[0] = '\0';
	pw_shell[0] = '\0';
	clear_errno ();
	if ((b = (name == NULL ? getpwuid (uid) : getpwnam (name))) != NULL)
	{
		*pw_uid = b -> pw_uid;
		*pw_gid = b -> pw_gid;
		if (strncopy (pw_name, b -> pw_name, 64) ||
			strncopy (pw_passwd, b -> pw_passwd, 64) ||
			strncopy (pw_gecos, b -> pw_gecos, 256) ||
			strncopy (pw_dir, b -> pw_dir, 1024) ||
			strncopy (pw_shell, b -> pw_shell, 1024))
				return ERANGE;
	}
	else
	{
		/*
		 * This the case:
		 *   "The given name or uid was not found."
		 * so errno should get
		 *   "0 or ENOENT or ESRCH or EBADF or EPERM or ..."
		 * but it doesn't (libc 2.13-26), so it must be set manually
		 *
		 */
		errno = ENOENT;
	}
	return errno;
}

gtm_status_t
posix_getpwnam (int argc,
	gtm_char_t *name,
	gtm_char_t *pw_name /* [64] */, gtm_char_t *pw_passwd /* [64] */,
	gtm_ulong_t *pw_uid, gtm_ulong_t *pw_gid,
	gtm_char_t *pw_gecos /* [256] */, gtm_char_t *pw_dir /* [1024] */, gtm_char_t *pw_shell /* [1024] */)
{
	return _posix_getpw (argc, name, 0, pw_name, pw_passwd, pw_uid, pw_gid, pw_gecos, pw_dir, pw_shell);
}

gtm_status_t
posix_getpwuid (int argc,
	gtm_ulong_t uid,
	gtm_char_t *pw_name /* [64] */, gtm_char_t *pw_passwd /* [64] */,
	gtm_ulong_t *pw_uid, gtm_ulong_t *pw_gid,
	gtm_char_t *pw_gecos /* [256] */, gtm_char_t *pw_dir /* [1024] */, gtm_char_t *pw_shell /* [1024] */)
{
	return _posix_getpw (argc, NULL, uid, pw_name, pw_passwd, pw_uid, pw_gid, pw_gecos, pw_dir, pw_shell);
}

static gtm_status_t
_posix_getgr (int argc,
	gtm_char_t *name,
	gtm_ulong_t gid,
	gtm_char_t *gr_name /* [64] */, gtm_char_t *gr_passwd /* [64] */,
	gtm_ulong_t *gr_gid, gtm_char_t *gr_mem /* [4096] */)
{
	struct group *g;
	int i;
	ssize_t l;
	char *p;
	check_argc (5);
	*gr_gid = 0;
	gr_name[0] = '\0';
	gr_passwd[0] = '\0';
	gr_mem[0] = '\0';
	clear_errno ();
	if ((g = (name == NULL) ? getgrgid (gid) : getgrnam (name)) != NULL)
	{
		*gr_gid = g -> gr_gid;
		if (strncopy (gr_name, g -> gr_name, 64) ||
			strncopy(gr_passwd, g -> gr_passwd, 64))
				return ERANGE;

		p = gr_mem; 
		l = 4096;
		for (i = 0; g -> gr_mem[i] != NULL; i++)
		{
			if (i > 0)
				if ((p = strpncopy (p, list_delimiter, &l)) == NULL)
					return ERANGE;
			if ((p = strpncopy (p, g -> gr_mem[i], &l)) == NULL)
				return ERANGE;
		}
	}
	else
	{
		errno = ENOENT;
	}
	return errno;
}

gtm_status_t
posix_getgrnam (int argc,
	gtm_char_t *name,
	gtm_char_t *gr_name /* [64] */, gtm_char_t *gr_passwd /* [64] */,
	gtm_ulong_t *gr_gid, gtm_char_t *gr_mem /* [4096] */)
{
	return _posix_getgr (argc, name, 0, gr_name, gr_passwd, gr_gid, gr_mem);
}

gtm_status_t
posix_getgrgid (int argc,
	gtm_ulong_t gid,
	gtm_char_t *gr_name /* [64] */, gtm_char_t *gr_passwd /* [64] */,
	gtm_ulong_t *gr_gid, gtm_char_t *gr_mem /* [4096] */)
{
	return _posix_getgr (argc, NULL, gid, gr_name, gr_passwd, gr_gid, gr_mem);
}

/* BSD getgrouplist like function (non-POSIX) */
gtm_status_t
posix_getgrouplist (int argc, gtm_char_t *name, gtm_char_t *list /* [4096] */)
{
	struct group *b;
	int i;
	ssize_t l = 4096;
	char *p = list;
	int found = 0;
	clear_errno ();
	list[0] = '\0';
	while ((b = getgrent ()) != NULL)
	{
		for (i = 0; b -> gr_mem[i] != NULL; i++)
			if (strcmp (b -> gr_mem[i], name) == 0)
			{
				if (found)
				{
					if ((p = strpncopy (p, list_delimiter, &l)) == NULL)
					{
						endgrent ();
						return ERANGE;
					}
				}
				else
					found++;
				if ((p = strpncopy (p, b -> gr_name, &l)) == NULL)
				{
					endgrent ();
					return ERANGE;
				}
			}
	}
	endgrent ();
	return errno;
}

#define dir_limit 256
static DIR *dir_list[dir_limit];
static int dir_count = 0;

#define push_dir(x) push_list ((void *) x, dir_list, dir_count)
#define check_dir() check_list (dir_count, dir_limit)
#define is_dir(x,y) in_list ((void *) x, (void **)dir_list, &dir_count, y)

gtm_status_t
posix_opendir (int argc, gtm_char_t *path, gtm_ulong_t *dir)
{
	DIR *b;
	check_argc (2);
	check_dir ();
	clear_errno ();
	if ((b = opendir (path)) != NULL)
	{
		*dir = (gtm_ulong_t) b;
		push_dir (b);
	}
	return errno;
}

gtm_status_t
posix_readdir (int argc, gtm_ulong_t dir, gtm_char_t *name /* [256] */)
{
	struct dirent *b;
	check_argc (2);
	name[0] = '\0';
	if (!is_dir (dir, 0))
		return EINVAL;
	clear_errno ();
	if ((b = readdir ((DIR *) dir)) != NULL)
		if (strncopy (name, b -> d_name, 256))
			return ERANGE;
	return errno;
}

gtm_status_t
posix_closedir (int argc, gtm_ulong_t dir)
{
	check_argc (1);
	if (!is_dir (dir, 1))
		return EINVAL;
	closedir ((DIR *) dir);
	return errno;
}

