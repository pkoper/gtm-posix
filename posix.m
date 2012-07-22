;
;  GT.M POSIX Extension
;  Copyright (C) 2012 Piotr Koper <piotr.koper@gmail.com>
;
;  This program is free software: you can redistribute it and/or modify
;  it under the terms of the GNU Affero General Public License as
;  published by the Free Software Foundation, either version 3 of the
;  License, or (at your option) any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU Affero General Public License for more details.
;
;  You should have received a copy of the GNU Affero General Public License
;  along with this program.  If not, see <http://www.gnu.org/licenses/>.
;

;  GT.M™ is a trademark of Fidelity Information Services, Inc.

; "GT.M™ is a vetted industrial strength, transaction processing application
;  platform consisting of a key-value database engine optimized for extreme
;  transaction processing throughput & business continuity."
;                                -- http://sourceforge.net/projects/fis-gtm/


;
; Non-zero errno will raise an exception in all routines except stat and lstat.
;
; stat and lstat does not raise an exception to let the M code stay clear when
; the tests for file/directory existence are concerned.
;
; This behaviour can be modified by changing function types in posix.xc file:
; 1) "gtm_status_t" to "gtm_int_t" to disable raising the exception,
; 2) "gtm_int_t" to "gtm_status_t" to enable raising the exception.
;

posix
	q

; Errors
; the C extension always clears errno before external library call, therefore
; errno can always be tested for errors

strerror(e) ; MUMPS-ish version
	q $s(e:$zm(e),1:"")


; Time

; w $$time^posix
;
time() ; returns unix time (UTC)
	q $&posix.time()

localtime(n,t) ; returns (local TZ) date components in n array, t is optional
	n q,sec,min,hour,mday,mon,year,wday,yday,isdst
	k n
	s errno=$&posix.localtime($g(t,$$time),.sec,.min,.hour,.mday,.mon,.year,.wday,.yday,.isdst)
	s:'errno n("sec")=sec,n("min")=min,n("hour")=hour,n("mday")=mday,n("mon")=mon,n("year")=year,n("wday")=wday,n("yday")=yday,n("isdst")=isdst
	q

gmtime(n,t) ; same as localtime() but in UTC (not localized)
	n sec,min,hour,mday,mon,year,wday,yday,isdst
	k n
	s errno=$&posix.gmtime($g(t,$$time),.sec,.min,.hour,.mday,.mon,.year,.wday,.yday,.isdst)
	s:'errno n("sec")=sec,n("min")=min,n("hour")=hour,n("mday")=mday,n("mon")=mon,n("year")=year,n("wday")=wday,n("yday")=yday,n("isdst")=isdst
	q

; d clktime^posix("MONOTONIC",.s,.ns)
; zwr
;
clktime(clkid,sec,nsec) ; better gettimeofday
	; clkid: "REALTIME", "MONOTONIC", "MONOTONIC_RAW", "PROCESS_CPUTIME_ID" or "THREAD_CPUTIME_ID" (case insensitive)
	s errno=$&posix.clockgettime(.clkid,.sec,.nsec)
	q

clkres(clkid,sec,nsec)
	; clkid: "REALTIME", "MONOTONIC", "MONOTONIC_RAW", "PROCESS_CPUTIME_ID" or "THREAD_CPUTIME_ID" (case insensitive)
	s errno=$&posix.clockgetres(.clkid,.sec,.nsec)
	q

; d localtime^posix(.n)
; w $$strftime^posix("%T %F",.n)
;
strftime(fmt,n) ; returns formated date
	n s
	s errno=$&posix.strftime(.fmt,n("sec"),n("min"),n("hour"),n("mday"),n("mon"),n("year"),n("wday"),n("yday"),n("isdst"),.s)
	q s

; d localtime^posix(.n)
; w $$mktime^posix(.n)
;
mktime(n) ; returns unix time
	q $&posix.mktime(n("sec"),n("min"),n("hour"),n("mday"),n("mon"),n("year"),n("wday"),n("yday"),n("isdst"))


; Environment

setenv(name,value,overwrite)
	; overwrite is optional
	d &posix.setenv(.name,.value,$g(overwrite,1))
	q

unsetenv(name)
	d &posix.unsetenv(.name)
	q


; System

times(n) ; CPU times
	n utime,stime,cutime,cstime
	k n
	s errno=$&posix.times(.utime,.stime,.cutime,.cstime)
	s:'errno n("utime")=utime,n("stime")=stime,n("cutime")=cutime,n("cstime")=cstime
	q

sysinfo(n) ; Load Average, memory, etc.
	k n
	n uptime,load1,load5,load15,totalram,freeram,sharedram,bufferram,totalswap,freeswap,procs,totalhigh,freehigh,unit
	s errno=$&posix.sysinfo(.uptime,.load1,.load5,.load15,.totalram,.freeram,.sharedram,.bufferram,.totalswap,.freeswap,.procs,.totalhigh,.freehigh,.unit)
	s:'errno n("load1")=load1,n("load5")=load5,n("load15")=load15,n("totalram")=totalram,n("freeram")=freeram,n("sharedram")=sharedram,n("bufferram")=bufferram,n("totalswap")=totalswap,n("freeswap")=freeswap,n("procs")=procs,n("totalhigh")=totalhigh,n("freehigh")=freehigh,n("unit")=unit
	q

uname(n)
	n sysname,nodename,release,version,machine
	s errno=$&posix.uname(.sysname,.nodename,.release,.version,.machine)
	s:'errno n("sysname")=sysname,n("nodename")=nodename,n("release")=release,n("version")=version,n("machine")=machine
	q


; Syslog

; d openlog^posix("TEST","PID|CONS","USER")
; d syslog^posix("ABC")
; d syslog^posix("DEF","EMERG")
;
openlog(ident,option,facility) ; optional setup for syslog
	; option: "|" joined "CONS", "NDELAY", "NOWAIT" or "PID", e.g. "PID|CONS" (case insensitive)
	; facility:	"AUTH", "AUTHPRIV", "CRON", "DAEMON", "FTP", "KERN",
	;		"LOCAL0", "LOCAL1", "LOCAL2", "LOCAL3", "LOCAL4", "LOCAL5", "LOCAL6", "LOCAL7",
	;		"LPR", "MAIL", "NEWS", "SYSLOG", "USER" or "UUCP" (case insensitive)
	s errno=$&posix.openlog(.ident,.option,.facility)
	q

syslog(message,priority)
	; priority: "EMERG", "ALERT", "CRIT", "ERR", "WARNING", "NOTICE", "INFO" or "DEBUG" (case insensitive, optional)
	d &posix.syslog($g(priority,"NOTICE"),.message)
	q


; File Permissions

mode(octal) ; returns integer permissions, takes octal representation
	n n,l,i
	s n=0,l=$l(octal) f i=l:-1:1 s n=n+($e(octal,i)*(8**(l-i)))
	q n

octal(mode) ; returns octal representation, takes integer permissions
	n s,i
	f i=1:1:4 s $e(s,5-i)=mode#8,mode=mode\8
	q s

; d chmod^posix("/tmp/a","0644")
; d chmod^posix("/tmp/a",644)
;
chmod(path,mode)
	s errno=$&posix.chmod(.path,$$mode(.mode))
	q

; for obtaining uid & gid see getpwnam^posix
chown(path,uid,gid)
	s errno=$&posix.chown(.path,.uid,.gid)
	q

lchown(path,uid,gid)
	s errno=$&posix.lchown(.path,.uid,.gid)
	q


; File & Directory

; see umask
mkdir(dir,mode)
	s errno=$&posix.mkdir(.dir,$$mode($g(mode,755)))
	q

rmdir(dir)
	s errno=$&posix.rmdir(.dir)
	q

unlink(path)
	s errno=$&posix.unlink(.path)
	q

link(oldpath,newpath)
	s errno=$&posix.link(.oldpath,.newpath)
	q

; d symlink^posix("/etc/passwd","/tmp/s1")
; w $$readlink^posix("/tmp/s1")
;
symlink(oldpath,newpath)
	s errno=$&posix.symlink(.oldpath,.newpath)
	q

readlink(path)
	n p
	s errno=$&posix.readlink(.path,.p)
	q p

umask(mask) ; returns umask mode in octal representation, takes octal representation
	q $$octal($&posix.umask($$mode(.mask)))

zbit(n,l) ; coverts l byte long M's integer to GT.M's zbitstring in big endian order
	n i,b
	f i=l:-1:1 s $ze(b,i)=$zch(n#256),n=n\256
	q $zch(0)_b
	
isflag(mode,flag)
	n s
	s s=$$zbit($$mode(.flag),4)
	q $zbitand($$zbit(.mode,4),s)=s

; M equivalent to POSIX macros for checking file type using stat's mode field (S_ISREG(m), S_ISDIR(m), ...)
; use on "mode" field returned from stat^posix
isreg(mode)  q $$isflag(.mode,0100000) ; regular file
isdir(mode)  q $$isflag(.mode,0040000) ; directory
ischr(mode)  q $$isflag(.mode,0020000) ; character device
isblk(mode)  q $$isflag(.mode,0060000) ; block device
isfifo(mode) q $$isflag(.mode,0010000) ; FIFO
islnk(mode)  q $$isflag(.mode,0120000) ; symbolic link
issock(mode) q $$isflag(.mode,0140000) ; socket

; d stat^posix("/etc/passwd",.n)
; w $$octal^posix(n("mode"))
; w $$isreg^posix(n("mode"))
; w $$isdir^posix(n("mode"))
;
stat(path,n) ; use $$is...^posix() and $$octal^posix() for "mode" field
	n dev,ino,mode,nlink,uid,gid,rdev,size,blksize,blocks,atime,mtime,ctime
	k n
	s errno=$&posix.stat(.path,.dev,.ino,.mode,.nlink,.uid,.gid,.rdev,.size,.blksize,.blocks,.atime,.mtime,.ctime)
	s:'errno n("dev")=dev,n("ino")=ino,n("mode")=mode,n("nlink")=nlink,n("uid")=uid,n("gid")=gid,n("rdev")=rdev,n("size")=size,n("blksize")=blksize,n("blocks")=blocks,n("atime")=atime,n("mtime")=mtime,n("ctime")=ctime
	q

lstat(path,n)
	n dev,ino,mode,nlink,uid,gid,rdev,size,blksize,blocks,atime,mtime,ctime
	k n
	s errno=$&posix.lstat(.path,.dev,.ino,.mode,.nlink,.uid,.gid,.rdev,.size,.blksize,.blocks,.atime,.mtime,.ctime)
	s:'errno n("dev")=dev,n("ino")=ino,n("mode")=mode,n("nlink")=nlink,n("uid")=uid,n("gid")=gid,n("rdev")=rdev,n("size")=size,n("blksize")=blksize,n("blocks")=blocks,n("atime")=atime,n("mtime")=mtime,n("ctime")=ctime
	q

; s dir=$$opendir^posix("/etc")
; f  s name=$$readdir^posix(.dir) q:name=""  w name,!
; d closedir^posix(.dir)
;
opendir(path)
	n dir
	s errno=$&posix.opendir(.path,.dir)
	q dir

readdir(dir)
	n name
	s errno=$&posix.readdir(.dir,.name)
	q name

closedir(dir)
	s errno=$&posix.closedir(.dir)
	q

; non-POSIX, utility function like mkdir -p
mkpath(path)
	n l,i,s
	s l=$l(path,"/") f i=1:1:l s s=$p(path,"/",1,i) d:s'=""
	. d stat(s)
	. d:errno mkdir($p(path,"/",1,i)) q:errno
	q

; non-POSIX, utility function like rm -r
rmpath(path)
	n n,s
	d lstat(path,.n) q:errno
	i $$isdir(n("mode")) d  q:errno
	. n d
	. s d=$$opendir(.path) q:errno
	. f  s s=$$readdir(.d) q:s=""  d:s'="."&(s'="..") rmpath(path_"/"_s) q:errno
	. d closedir(.d)
	. d rmdir(.path)
	e  d:'errno unlink(.path)
	q


; Password File

; d getpwnam^posix("root",.n)
; zwr
;
getpwnam(user,n)
	n name,passwd,uid,gid,gecos,dir,shell
	k n
	s errno=$&posix.getpwnam(.user,.name,.passwd,.uid,.gid,.gecos,.dir,.shell)
	s:'errno n("name")=name,n("passwd")=passwd,n("uid")=uid,n("gid")=gid,n("dir")=dir,n("shell")=shell,n("gecos","fullname")=$p(gecos,",",1),n("gecos","office")=$p(gecos,",",2),n("gecos","workphone")=$p(gecos,",",3),n("gecos","homephone")=$p(gecos,",",4)
	q

getpwuid(id,n)
	n name,passwd,uid,gid,gecos,dir,shell
	k n
	s errno=$&posix.getpwuid(.id,.name,.passwd,.uid,.gid,.gecos,.dir,.shell)
	s:'errno n("name")=name,n("passwd")=passwd,n("uid")=uid,n("gid")=gid,n("dir")=dir,n("shell")=shell,n("gecos","fullname")=$p(gecos,",",1),n("gecos","office")=$p(gecos,",",2),n("gecos","workphone")=$p(gecos,",",3),n("gecos","homephone")=$p(gecos,",",4)
	q

getgrnam(group,n)
	n name,passwd,gid,mem
	k n
	s errno=$&posix.getgrnam(.group,.name,.passwd,.gid,.mem)
	s:'errno n("name")=name,n("passwd")=passwd,n("gid")=gid,n("mem")=mem
	q

getgrgid(id,n)
	n name,passwd,gid,mem
	k n
	s errno=$&posix.getgrgid(.id,.name,.passwd,.gid,.mem)
	s:'errno n("name")=name,n("passwd")=passwd,n("gid")=gid,n("mem")=mem
	q

; w $$getgrouplist^posix("root")
;
getgrouplist(user) ; non-POSIX, portable version
	n list
	s errno=$&posix.getgrouplist(.user,.list)
	q list

