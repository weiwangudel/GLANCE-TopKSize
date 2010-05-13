## Process this file with automake to produce Makefile.in
## Created by Anjuta

SUBDIRS = src po

topk_bound_estdocdir = ${prefix}/doc/topk_bound_est
topk_bound_estdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS


INTLTOOL_FILES = intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

EXTRA_DIST = $(topk_bound_estdoc_DATA) \
	$(INTLTOOL_FILES)

DISTCLEANFILES = intltool-extract \
	intltool-merge \
	intltool-update \
	po/.intltool-merge-cache


# Remove doc directory on uninstall
uninstall-local:
	-rm -r $(topk_bound_estdocdir)
