lib_LTLIBRARIES = src/libregutils.la
include_HEADERS = $(top_srcdir)/include/regutils.h
src_libregutils_la_SOURCES = src/regutils.c src/vector.h
src_libregutils_la_CPPFLAGS = -I$(top_srcdir)/include
src_libregutils_la_LDFLAGS = -version-info 1:0:0
noinst_PROGRAMS = examples/demo
examples_demo_SOURCES = examples/demo.c
examples_demo_CPPFLAGS = -I$(top_srcdir)/include
examples_demo_LDADD = src/libregutils.la
dist_man3_MANS = man/preg_init.3 man/preg_free.3 man/preg_setopt.3 \
man/preg_delopt.3 man/preg_so.3 man/preg_eo.3 man/preg_errcode.3 \
man/preg_errmsg.3 man/preg_escape.3 man/preg_getmatch.3 man/preg_getrep.3 \
man/preg_getsplit.3 man/preg_matc.3 man/preg_match.3 man/preg_matchlen.3 \
man/preg_replace.3 man/preg_replen.3 man/preg_split.3 man/preg_splitc.3 \
man/preg_splitlen.3 man/preg_subc.3
