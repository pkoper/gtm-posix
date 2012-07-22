# successfully compiled with gcc version 4.6.3 on Debian 6 in GT.M user
# environment
#
all:
	gcc -Wall -Werror -pedantic -fPIC -shared -o libposix.so posix.c -I$(gtm_dist)

# Example installation procedure
#
# See http://tinco.pair.com/bhaskar/gtm/doc/books/pg/UNIX_manual/ch11s04.html#examples_fo_using_ext_calls
# for details
#
# NOTES:
#	The $(gtm_dist)/plugin directory and the plugins functionality is
#	available since GT.M V5.5-000, for older GT.M versions place posix.m
#	in your routines directory, and posix.env and libposix.so in a
#	directory of your choice, and update posix.xc and .profile accordingly
#	to point those files.
#
#	The $(gtm_dist)/plugin/o directory should be writable the GT.M user.
#
#	The script will succeed if your version of GT.M is V5.5-000 or above,
#	and the GT.M user has an access to recursively modify the default
#	permissions for $(gtm_dist)/plugin, you can simply run
#		chown -R gtm $(gtm_dist)/plugin
#	as root to achieve that (change the "gtm" to your GT.M system user).
#
install:
	chmod u+w -R $(gtm_dist)/plugin
	cp posix.m $(gtm_dist)/plugin/r/
	cp libposix.so posix.xc posix.env $(gtm_dist)/plugin/
	sed -i "s|gtm_dist|$(gtm_dist)|" $(gtm_dist)/plugin/posix.xc $(gtm_dist)/plugin/posix.env
	echo ". $(gtm_dist)/plugin/posix.env" >> ~/.profile
