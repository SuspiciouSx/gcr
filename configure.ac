AC_PREREQ([2.63])
AC_INIT([gcr], [3.4.1],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-keyring&component=gcr],
        [gcr])

dnl ****************************************************************************
dnl Library package and libtool versioning
dnl
dnl Updating the libtool versions, follow these instructions sequentially:
dnl  1. If the library source code has changed at all since the last update, then increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
dnl  2. If any interfaces have been added, removed, or changed since the last update, increment current, and set revision to 0.
dnl  3. If any interfaces have been added since the last public release, then increment age.
dnl  4. If any interfaces have been removed or changed since the last public release, then set age to 0.

# GCK package versioning
GCK_MAJOR=1
GCK_MINOR=0
GCK_MICRO=0

# GCK libtool versioning
GCK_CURRENT=0
GCK_REVISION=0
GCK_AGE=0

# GCR package versioning
GCR_MAJOR=3
GCR_MINOR=0
GCR_MICRO=0

# GCR libtool versioning
GCR_CURRENT=1
GCR_REVISION=0
GCR_AGE=0

dnl ****************************************************************************

AC_CONFIG_MACRO_DIR([build/m4])
AM_INIT_AUTOMAKE([1.11 tar-ustar foreign -Wno-portability])
AM_SANITY_CHECK
AM_CONFIG_HEADER(config.h)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([enable])

# -----------------------------------------------------------------------------
# Basic tools

AC_GNU_SOURCE
AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_DISABLE_STATIC
AM_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.35.0])
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

GETTEXT_PACKAGE=gcr
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [The gettext domain name])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT

# -----------------------------------------------------------------------------
# GLib and GTK+ stuff

PKG_CHECK_MODULES(GLIB,
	glib-2.0 >= 2.30.0
	gmodule-no-export-2.0
	gthread-2.0
	gobject-2.0
	gio-2.0 gio-unix-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

GLIB_GSETTINGS
GOBJECT_INTROSPECTION_CHECK([1.29])
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)

GTK_DOC_CHECK(1.9)

PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 3.0)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

# --------------------------------------------------------------------
# Checks for functions
#

AC_CHECK_FUNCS(timegm, AC_DEFINE(HAVE_TIMEGM,1,[Have timegm]))
AC_CHECK_FUNCS(mlock)

# --------------------------------------------------------------------
# p11-kit

PKG_CHECK_MODULES([P11_KIT], p11-kit-1 >= 0.6)
AC_SUBST(P11_KIT_CFLAGS)
AC_SUBST(P11_KIT_LIBS)

AC_MSG_CHECKING([where to put pkcs11 module configuration])
P11_SYSTEM_CONFIG_MODULES=$($PKG_CONFIG p11-kit-1 --variable=p11_system_config_modules)
AC_MSG_RESULT($P11_SYSTEM_CONFIG_MODULES)
AC_SUBST(P11_SYSTEM_CONFIG_MODULES)

if test "$P11_SYSTEM_CONFIG_MODULES" = ""; then
	AC_MSG_ERROR([Could not find location for pkcs11 module config])
fi

AC_MSG_CHECKING([module path to install pkcs11 modules])
AC_ARG_WITH(pkcs11-modules, [
            AC_HELP_STRING([--with-pkcs11-modules=<dir>],
                           [directory to install PKCS#11 modules])
            ])
if test -z "$with_pkcs11_modules" ; then
	P11_MODULE_PATH=$($PKG_CONFIG p11-kit-1 --variable=p11_module_path)
else
	P11_MODULE_PATH="$with_pkcs11_modules"
fi
if test "$P11_MODULE_PATH" = ""; then
	AC_MSG_ERROR([Could not find directory to install pkcs11 modules])
fi
AC_MSG_RESULT($P11_MODULE_PATH)
AC_SUBST(P11_MODULE_PATH)

pkcs11standalonedir="${libdir}/gnome-keyring/devel"
AC_SUBST(pkcs11standalonedir)

# --------------------------------------------------------------------
# GPG support

AC_PATH_PROGS([GNUPG], [gpg gpg2], ["gpg"])
AC_DEFINE_UNQUOTED([GPG_EXECUTABLE], ["$GNUPG"], [Path to gpg executable.])

# --------------------------------------------------------------------
# libgcrypt
#

GCRYPT_VERSION=1.2.2
GCRYPT_LIBVER=1

AM_PATH_LIBGCRYPT($GCRYPT_LIBVER:$GCRYPT_VERSION,,
	AC_MSG_ERROR([[
***
*** libgcrypt was not found. You may want to get it from
*** ftp://ftp.gnupg.org/gcrypt/libgcrypt/
***
]]))
libgcrypt=yes
AC_DEFINE_UNQUOTED(LIBGCRYPT_VERSION, "$GCRYPT_VERSION", [Version of GCRYPT we expect])

AC_SUBST([LIBGCRYPT_CFLAGS])
AC_SUBST([LIBGCRYPT_LIBS])

# -------------------------------------------------------------------
# libtasn1
#

AC_PATH_PROG(ASN1PARSER, asn1Parser, no)
if test "$ASN1PARSER" = "no" ; then
	AC_MSG_ERROR([asn1Parser tool is not installed, try libtasn1 or a related package])
fi

# ----------------------------------------------------------------------
# mime-database

AC_ARG_ENABLE(update-mime,
	AC_HELP_STRING([--disable-update-mime],
	[don't run update-mime-database utility (useful for packages) ]))
AM_CONDITIONAL(WITH_UPDATE_MIME, test "$enable_update_mime" != "no")

AC_ARG_ENABLE(update-icon-cache,
	AC_HELP_STRING([--disable-update-icon-cache],
	[don't run gtk-update-icon-cache utility (useful for packages) ]))
AM_CONDITIONAL(WITH_UPDATE_ICON_CACHE, test "$enable_update_icon_cache" != "no")

# ----------------------------------------------------------------------
# DBus services

AC_ARG_WITH(dbus-services,
		  [AC_HELP_STRING([--with-dbus-services=<dir>],
		  [where D-BUS session services directory is])])
if ! test -z "$with_dbus_services" ; then
	DBUS_SERVICES_DIR="$with_dbus_services"
else
	DBUS_SERVICES_DIR="$datadir/dbus-1/services"
fi

AC_SUBST(DBUS_SERVICES_DIR)

# --------------------------------------------------------------------
# Compilation and linking options
#

AC_MSG_CHECKING([for debug mode])
AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug=no/default/yes],
              [Turn on or off debugging]))

if test "$enable_debug" != "no"; then
	AC_DEFINE_UNQUOTED(WITH_DEBUG, 1, [Print debug output])
	AC_DEFINE_UNQUOTED(_DEBUG, 1, [In debug mode])
	CFLAGS="$CFLAGS -g"
fi

if test "$enable_debug" = "yes"; then
	debug_status="yes (-g, -O0, debug output, testable)"
	AC_DEFINE_UNQUOTED(GCR_DISABLE_GRABS, 1, [Disable grabs])
	CFLAGS="$CFLAGS -O0"
elif test "$enable_debug" = "no"; then
	debug_status="no (no debug output, not testable, G_DISABLE_ASSERT)"
	AC_DEFINE_UNQUOTED(G_DISABLE_ASSERT, 1, [Disable glib assertions])
else
	debug_status="default (-g, debug output, testable)"
fi

AC_MSG_RESULT($debug_status)

AC_MSG_CHECKING(for more warnings)
if test "$GCC" = "yes"; then
	AC_MSG_RESULT(yes)
	CFLAGS="\
	-Wall \
	-Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
	-Wnested-externs -Wpointer-arith \
	-Wcast-align -Wsign-compare \
	$CFLAGS"

	# Disable -Werror for now, because of issues with
	# strange gtk-doc programs not building cleanly.
	# -Werror \

	for option in -Wno-strict-aliasing -Wno-sign-compare; do
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $option"
		AC_MSG_CHECKING([whether gcc understands $option])
		AC_TRY_COMPILE([], [],
			has_option=yes,
			has_option=no,)
		if test $has_option = no; then
			CFLAGS="$SAVE_CFLAGS"
		fi
		AC_MSG_RESULT($has_option)
		unset has_option
		unset SAVE_CFLAGS
	done
	unset option
else
	AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(strict, [
               AS_HELP_STRING([--enable-strict], [Strict code compilation])
             ])

AC_MSG_CHECKING([build strict])

if test "$enable_strict" = "yes"; then
	CFLAGS="$CFLAGS -Werror \
		-DGTK_DISABLE_DEPRECATED \
		-DGDK_DISABLE_DEPRECATED \
		-DG_DISABLE_DEPRECATED \
		-DGDK_PIXBUF_DISABLE_DEPRECATED"
	TEST_MODE="thorough"
	INTROSPECTION_FLAGS="--warn-error"
	AC_DEFINE_UNQUOTED(WITH_STRICT, 1, [More strict checks])
	strict_status="yes (-Werror, thorough tests, fatals, no deprecations)"
else
	TEST_MODE="quick"
	INTROSPECTION_FLAGS=""
	strict_status="no (quick tests, non-fatal warnings)"
fi

AC_MSG_RESULT($strict_status)
AC_SUBST(INTROSPECTION_FLAGS)
AC_SUBST(TEST_MODE)

AC_MSG_CHECKING([whether to build with gcov testing])
AC_ARG_ENABLE([coverage],
		AS_HELP_STRING([--enable-coverage],
			[Whether to enable coverage testing ]),
		[],
		[enable_coverage=no])

AC_MSG_RESULT([$enable_coverage])

if test "$enable_coverage" = "yes"; then
	if test "$GCC" != "yes"; then
		AC_MSG_ERROR(Coverage testing requires GCC)
	fi

	AC_PATH_PROG(GCOV, gcov, no)
	if test "$GCOV" = "no" ; then
		AC_MSG_ERROR(gcov tool is not available)
	fi

	AC_PATH_PROG(LCOV, lcov, no)
	if test "$LCOV" = "no" ; then
		AC_MSG_ERROR(lcov tool is not installed)
	fi

	AC_PATH_PROG(GENHTML, genhtml, no)
	if test "$GENHTML" = "no" ; then
		AC_MSG_ERROR(lcov's genhtml tool is not installed)
	fi

	CFLAGS="$CFLAGS -O0 -g -fprofile-arcs -ftest-coverage"
	LDFLAGS="$LDFLAGS -lgcov"
fi

AM_CONDITIONAL([WITH_COVERAGE], [test "$enable_coverage" = "yes"])
AC_SUBST(LCOV)
AC_SUBST(GCOV)
AC_SUBST(GENHTML)

AC_ARG_ENABLE(valgrind,
	AC_HELP_STRING([--enable-valgrind],
	[Run gnome-keyring-daemon using valgrind]))

AC_MSG_CHECKING([valgrind])
valgrind_status="yes"

AM_CONDITIONAL(WITH_VALGRIND, test "$enable_valgrind" = "run")

# Run gnome-keyring-daemon under valgrind as default
if test "$enable_valgrind" = "run"; then
	AC_PATH_PROG(VALGRIND, valgrind, no)
	if test "$VALGRIND" = "no" ; then
		AC_MSG_ERROR(valgrind tool is not installed)
	fi

	VALGRIND_ARG="--log-file=/tmp/gkr-valgrind.log.%p"
	VALGRIND_RUN="$VALGRIND $VALGRIND_ARG "

	AC_DEFINE_UNQUOTED(VALGRIND, "$VALGRIND", [Path to valgrind executable])
	AC_DEFINE_UNQUOTED(VALGRIND_ARG, "$VALGRIND_ARG", [Valgrind arguments])

	enable_valgrind="yes"
	valgrind_status="run"
fi

AC_SUBST(VALGRIND)
AC_SUBST(VALGRIND_ARG)
AC_SUBST(VALGRIND_RUN)

# Build valgrind support into code
if test "$enable_valgrind" = "yes"; then
	AC_CHECK_HEADER([valgrind/valgrind.h], have_valgrind=yes, have_valgrind=no)
	if test "$have_valgrind" = "no"; then
		AC_MSG_ERROR(The valgrind headers are missing)
	fi
	AC_DEFINE_UNQUOTED(WITH_VALGRIND, 1, [Run under valgrind])
	AC_MSG_RESULT(yes)


# No valgrind
else
	AC_MSG_RESULT(no)
	valgrind_status="no"
fi

# ----------------------------------------------------------------------

GCK_LT_RELEASE=$GCK_CURRENT:$GCK_REVISION:$GCK_AGE
AC_SUBST(GCK_LT_RELEASE)
AC_SUBST(GCK_MAJOR)
AC_SUBST(GCK_MINOR)

GCR_LT_RELEASE=$GCR_CURRENT:$GCR_REVISION:$GCR_AGE
AC_SUBST(GCR_LT_RELEASE)
AC_SUBST(GCR_MAJOR)
AC_SUBST(GCR_MINOR)

AC_OUTPUT([
Makefile
build/Makefile
docs/Makefile
docs/reference/Makefile
docs/reference/gck/Makefile
docs/reference/gck/version.xml
docs/reference/gcr/Makefile
docs/reference/gcr/version.xml
egg/Makefile
egg/tests/Makefile
gck/gck.pc
gck/Makefile
gck/tests/Makefile
gcr/gcr.pc
gcr/gcr-base.pc
gcr/Makefile
gcr/icons/Makefile
gcr/icons/16x16/Makefile
gcr/icons/22x22/Makefile
gcr/icons/24x24/Makefile
gcr/icons/32x32/Makefile
gcr/icons/48x48/Makefile
gcr/icons/256x256/Makefile
gcr/tests/Makefile
po/Makefile.in
schema/Makefile
testing/Makefile
])

# ------------------------------------------------------------------------------
# Summary
#

echo
echo "BUILD"
echo "  Debug Build:          $debug_status"
echo "  Strict Compilation:   $strict_status"
echo "  Introspection:        $found_introspection"
echo "  Valgrind:             $valgrind_status"
echo "  Test Coverage:        $enable_coverage"
echo
