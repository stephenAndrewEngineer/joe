# CMake build for JOE (Joe's Own Editor).
#
# Out-of-tree build:
#   cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
#   cmake --build build
#   cmake --install build
#
# The platform checks below generate autoconf.h in the build tree, so the
# source tree is never written to.

cmake_minimum_required(VERSION 3.13)

project(joe VERSION 4.6 LANGUAGES C)

# Default to an optimized build with debug info (-g -O2)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCSourceRuns)

# A stale joe/autoconf.h in the source tree (left by an old autotools build)
# would shadow the one generated here (config.h includes it with "").
if(EXISTS "${PROJECT_SOURCE_DIR}/joe/autoconf.h")
	message(FATAL_ERROR
		"${PROJECT_SOURCE_DIR}/joe/autoconf.h is left over from an old autotools "
		"build and would shadow the generated header. Delete it first.")
endif()

#### Options ####

option(JOE_CURSES "Search the curses libraries for terminfo support" ON)
option(JOE_TERMCAP "Search the termcap library for terminfo support" ON)
option(JOE_BUILD_UTILS "Build the helper programs in joe/util" OFF)
option(JOE_MATH_FUNCS "Math functions (sin, sqrt, ^, regression, ...) in the calculator; links libm" OFF)
set(JOE_UNICODE_VERSION "9.0.0" CACHE STRING "Unicode version of the joe/unicat-<version>.c table to build")

set(JOE_UNICAT "${PROJECT_SOURCE_DIR}/joe/unicat-${JOE_UNICODE_VERSION}.c")
if(NOT EXISTS "${JOE_UNICAT}")
	message(FATAL_ERROR "No Unicode table for JOE_UNICODE_VERSION=${JOE_UNICODE_VERSION}: ${JOE_UNICAT} does not exist")
endif()

#### Install locations ####

set(JOE_SYSCONFDIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/joe")
set(JOE_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}/joe")
set(JOE_DOCDIR "${CMAKE_INSTALL_FULL_DOCDIR}")

#### Library search ####

# Libraries found by joe_search_libs(); linked into joe.
set(JOE_LIBS "")

# joe_search_libs(<function> <result-var> <lib>...)
# Sets <result-var> if <function> links with no extra library or with one of
# the given libraries; in the latter case the library is added to JOE_LIBS.
function(joe_search_libs func var)
	string(TOUPPER "${func}" ufunc)
	set(CMAKE_REQUIRED_LIBRARIES ${JOE_LIBS})
	check_function_exists(${func} JOE_FUNC_${ufunc})
	if(JOE_FUNC_${ufunc})
		set(${var} 1 PARENT_SCOPE)
		return()
	endif()
	foreach(lib IN LISTS ARGN)
		string(TOUPPER "${lib}" ulib)
		check_library_exists(${lib} ${func} "" JOE_LIB_${ulib}_${ufunc})
		if(JOE_LIB_${ulib}_${ufunc})
			set(${var} 1 PARENT_SCOPE)
			list(APPEND JOE_LIBS ${lib})
			set(JOE_LIBS ${JOE_LIBS} PARENT_SCOPE)
			return()
		endif()
	endforeach()
endfunction()

joe_search_libs(openpty HAVE_OPENPTY util)
joe_search_libs(login_tty HAVE_LOGIN_TTY util)

set(JOE_TERM_LIBS "")
if(JOE_CURSES)
	list(APPEND JOE_TERM_LIBS ncurses tinfo curses)
endif()
if(JOE_TERMCAP)
	list(APPEND JOE_TERM_LIBS termcap)
endif()
joe_search_libs(tgetflag TERMINFO ${JOE_TERM_LIBS})

joe_search_libs(snprintf HAVE_SNPRINTF snprintf db)

# The calculator (umath.c) only uses libm when JOE_MATH_FUNCS is on
if(JOE_MATH_FUNCS)
	foreach(func sin cos tan exp sqrt cbrt log log10 asin acos atan sinh cosh tanh
	             asinh acosh atanh floor ceil fabs erf erfc j0 j1 y0 y1 hypot pow)
		string(TOUPPER "${func}" ufunc)
		joe_search_libs(${func} HAVE_${ufunc} m)
	endforeach()
	if(NOT HAVE_POW OR NOT HAVE_SQRT OR NOT HAVE_LOG OR NOT HAVE_EXP)
		message(FATAL_ERROR "JOE_MATH_FUNCS needs pow, sqrt, log and exp")
	endif()
endif()

#### Header files ####

foreach(header
		curses.h dirent.h errno.h fcntl.h libutil.h limits.h locale.h
		paths.h pty.h pwd.h sgtty.h signal.h stddef.h stdlib.h string.h
		sys/dirent.h sys/ioctl.h sys/param.h sys/stat.h sys/time.h
		sys/types.h sys/utime.h sys/wait.h time.h unistd.h utime.h utmp.h
		vfork.h selinux/selinux.h selinux/context.h)
	string(TOUPPER "HAVE_${header}" var)
	string(REGEX REPLACE "[/.]" "_" var "${var}")
	check_include_file(${header} ${var})
endforeach()

# Fall back to the older directory headers when there is no <dirent.h>
if(NOT HAVE_DIRENT_H)
	foreach(header sys/ndir.h sys/dir.h ndir.h)
		string(TOUPPER "HAVE_${header}" var)
		string(REGEX REPLACE "[/.]" "_" var "${var}")
		check_include_file(${header} ${var})
		if(${var})
			break()
		endif()
	endforeach()
endif()

include(CheckIncludeFiles)
if(HAVE_CURSES_H)
	check_include_files("curses.h;term.h" HAVE_TERM_H)
else()
	check_include_file(term.h HAVE_TERM_H)
endif()

if(HAVE_SELINUX_SELINUX_H AND HAVE_SELINUX_CONTEXT_H)
	set(HAVE_SELINUX 1)
endif()

# Terminal interface: POSIX termios, else SysV termio
check_symbol_exists(tcgetattr termios.h HAVE_POSIX_TERMIOS)
if(NOT HAVE_POSIX_TERMIOS)
	check_include_file(termio.h HAVE_TERMIO_H)
	check_include_file(sys/termio.h HAVE_SYS_TERMIO_H)
	if(HAVE_TERMIO_H AND HAVE_SYS_TERMIO_H)
		set(HAVE_SYSV_TERMIO 1)
	endif()
endif()

# <sys/ioctl.h> is only needed for TIOCGWINSZ when <termios.h> doesn't provide it
check_symbol_exists(TIOCGWINSZ "sys/types.h;termios.h" JOE_TIOCGWINSZ_IN_TERMIOS)
if(NOT JOE_TIOCGWINSZ_IN_TERMIOS)
	check_symbol_exists(TIOCGWINSZ "sys/types.h;sys/ioctl.h" GWINSZ_IN_SYS_IOCTL)
endif()

#### Types and sizes ####

check_type_size(char SIZEOF_CHAR)
check_type_size(short SIZEOF_SHORT)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size("long long" SIZEOF_LONG_LONG)
check_type_size("void *" SIZEOF_VOID_P)
if(HAVE_SIZEOF_LONG_LONG)
	set(HAVE_LONG_LONG 1)
endif()

set(CMAKE_EXTRA_INCLUDE_FILES sys/types.h)
check_type_size(off_t SIZEOF_OFF_T)
check_type_size(ssize_t SIZEOF_SSIZE_T)
set(CMAKE_EXTRA_INCLUDE_FILES signal.h)
check_type_size(sighandler_t SIZEOF_SIGHANDLER_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)

if(NOT HAVE_SIZEOF_SSIZE_T)
	set(ssize_t int)
endif()
if(HAVE_SIZEOF_SIGHANDLER_T)
	set(HAVE_SIGHANDLER_T 1)
endif()

#### Library functions ####

foreach(func alarm fork vfork getcwd getwd isblank mkdir mkstemp putenv
             setlocale strchr strdup utime setpgid setitimer sigaction
             sigvec siginterrupt sigprocmask lstat)
	string(TOUPPER "HAVE_${func}" var)
	check_function_exists(${func} ${var})
endforeach()

if(NOT HAVE_GETCWD AND NOT HAVE_GETWD)
	message(FATAL_ERROR "don't know how to get current working directory")
endif()

if(HAVE_FORK)
	set(HAVE_WORKING_FORK 1)
endif()
if(HAVE_VFORK)
	set(HAVE_WORKING_VFORK 1)
else()
	set(vfork fork)
endif()
if(HAVE_LSTAT AND UNIX)
	set(LSTAT_FOLLOWS_SLASHED_SYMLINK 1)
endif()

#### Run-time behaviour checks ####

if(CMAKE_CROSSCOMPILING)
	message(WARNING "Cross compiling: assuming defaults for run-time checks")
	set(HAVE_WORKING_ISBLANK "")
	set(SETPGRP_VOID 1)
	set(NEED_TO_REINSTALL_SIGNAL 1)
else()
	if(HAVE_ISBLANK)
		check_c_source_runs("
			#include <ctype.h>
			#include <stdlib.h>
			int main() {
				int a = 0;
				isblank(a++);
				exit(a != 1);
			}" HAVE_WORKING_ISBLANK)
	endif()

	# Succeeds if setpgrp() takes two args (*BSD systems)
	check_c_source_runs("
		#include <stdlib.h>
		#include <unistd.h>
		int main() {
			exit(setpgrp(0, 0) != 0);
		}" JOE_SETPGRP_TWO_ARGS)
	if(NOT JOE_SETPGRP_TWO_ARGS)
		set(SETPGRP_VOID 1)
	endif()

	set(CMAKE_REQUIRED_DEFINITIONS "")
	foreach(var HAVE_SIGACTION HAVE_SIGVEC HAVE_SIGHANDLER_T)
		if(${var})
			list(APPEND CMAKE_REQUIRED_DEFINITIONS -D${var}=1)
		endif()
	endforeach()
	# Succeeds if we don't have to reinstall the handler when invoked
	check_c_source_runs("
		#include <stdlib.h>
		#include <unistd.h>
		#include <signal.h>
		#ifndef HAVE_SIGHANDLER_T
		typedef void (*sighandler_t)(int);
		#endif
		int nsigint;
		void set_signal(int signum, sighandler_t handler) {
		#if HAVE_SIGACTION
			struct sigaction sact;
			sact.sa_handler = handler;
			sact.sa_flags = 0;
			sigemptyset(&sact.sa_mask);
			sigaction(signum, &sact, NULL);
		#elif HAVE_SIGVEC
			struct sigvec svec;
			svec.sv_handler = handler;
			svec.sv_flags = 0;
			sigemptyset(&svec.sv_mask);
			sigvec(signum, &svec, NULL);
		#else
			signal(signum, handler);
		#endif
		}
		void sigint(int s) { nsigint++; }
		int main() {
			nsigint = 0;
			set_signal(SIGINT, sigint);
			kill((int)getpid(), SIGINT);
			kill((int)getpid(), SIGINT);
			exit(nsigint != 2);
		}" JOE_SIGHANDLERS_PERSIST)
	unset(CMAKE_REQUIRED_DEFINITIONS)
	if(NOT JOE_SIGHANDLERS_PERSIST)
		set(NEED_TO_REINSTALL_SIGNAL 1)
	endif()
endif()

#### Spell checker used by the rc files ####

find_program(JOE_ASPELL aspell)
if(JOE_ASPELL)
	set(SPELL aspell)
else()
	set(SPELL ispell)
endif()

#### Generated files ####

configure_file(cmake/autoconf.h.cmake "${PROJECT_BINARY_DIR}/include/autoconf.h")

# joe_substitute(<input> <output>)
# Replaces the @JOERC@, @JOEDOC@, @JOEDATA@, @SPELL@ and @BINDIR@ placeholders
# with the install locations; other text is left untouched.
function(joe_substitute in out)
	file(READ "${in}" text)
	string(REPLACE "@JOERC@" "${JOE_SYSCONFDIR}" text "${text}")
	string(REPLACE "@JOEDOC@" "${JOE_DOCDIR}" text "${text}")
	string(REPLACE "@JOEDATA@" "${JOE_DATADIR}" text "${text}")
	string(REPLACE "@SPELL@" "${SPELL}" text "${text}")
	string(REPLACE "@BINDIR@" "${CMAKE_INSTALL_FULL_BINDIR}" text "${text}")
	file(WRITE "${out}" "${text}")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${in}")
endfunction()

set(JOE_RC_FILES joerc jicerc.ru jmacsrc jstarrc rjoerc jpicorc joerc.zh_TW)
set(JOE_RC_BUILT "")
foreach(rc IN LISTS JOE_RC_FILES)
	joe_substitute("${PROJECT_SOURCE_DIR}/rc/${rc}.in" "${PROJECT_BINARY_DIR}/rc/${rc}")
	list(APPEND JOE_RC_BUILT "${PROJECT_BINARY_DIR}/rc/${rc}")
endforeach()

joe_substitute("${PROJECT_SOURCE_DIR}/man/joe.1.in" "${PROJECT_BINARY_DIR}/man/joe.1")
joe_substitute("${PROJECT_SOURCE_DIR}/man/ru/joe.1.in" "${PROJECT_BINARY_DIR}/man/ru/joe.1")
joe_substitute("${PROJECT_SOURCE_DIR}/charmaps/klingon.in" "${PROJECT_BINARY_DIR}/charmaps/klingon")

#### The editor ####

add_executable(joe
	joe/b.c joe/blocks.c joe/bw.c joe/cmd.c joe/hash.c joe/help.c joe/kbd.c
	joe/macro.c joe/main.c joe/menu.c joe/path.c joe/poshist.c joe/pw.c
	joe/queue.c joe/qw.c joe/rc.c joe/regex.c joe/scrn.c joe/tab.c
	joe/termcap.c joe/tty.c joe/tw.c joe/ublock.c joe/uedit.c joe/uerror.c
	joe/ufile.c joe/uformat.c joe/uisrch.c joe/umath.c joe/undo.c
	joe/usearch.c joe/ushell.c joe/utag.c joe/va.c joe/vfile.c joe/vs.c
	joe/w.c joe/utils.c joe/syntax.c joe/utf8.c joe/selinux.c
	joe/charmap.c joe/mouse.c joe/lattr.c joe/gettext.c joe/builtin.c
	joe/builtins.c joe/vt.c joe/mmenu.c joe/state.c joe/options.c
	joe/unicode.c joe/cclass.c joe/frag.c joe/colors.c
	"${JOE_UNICAT}")

target_include_directories(joe PRIVATE
	"${PROJECT_BINARY_DIR}/include"
	"${PROJECT_SOURCE_DIR}/joe")

target_compile_definitions(joe PRIVATE
	HAVE_CONFIG_H
	JOERC="${JOE_SYSCONFDIR}/"
	JOEDATA="${JOE_DATADIR}/")

target_link_libraries(joe PRIVATE ${JOE_LIBS})

#### Helper programs (not installed) ####

if(JOE_BUILD_UTILS)
	foreach(util termidx stringify uniproc checkwidths)
		add_executable(${util} joe/util/${util}.c)
		target_include_directories(${util} PRIVATE
			"${PROJECT_BINARY_DIR}/include"
			"${PROJECT_SOURCE_DIR}/joe")
	endforeach()
	target_link_libraries(termidx PRIVATE ${JOE_LIBS})
endif()

#### Install ####

install(TARGETS joe RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# jmacs, jstar, rjoe and jpico are symlinks to joe; the name picks the rc file
foreach(alias jmacs jstar rjoe jpico)
	install(CODE "
		set(link \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/${alias}\")
		message(STATUS \"Installing: \${link}\")
		file(REMOVE \"\${link}\")
		file(CREATE_LINK joe \"\${link}\" SYMBOLIC)")
endforeach()

install(FILES ${JOE_RC_BUILT}
	rc/ftyperc rc/shell.sh rc/shell.csh
	DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/joe")

file(GLOB JOE_SYNTAX_FILES "${PROJECT_SOURCE_DIR}/syntax/*.jsf")
install(FILES ${JOE_SYNTAX_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/joe/syntax")

file(GLOB JOE_COLOR_FILES "${PROJECT_SOURCE_DIR}/colors/*.jcf")
install(FILES ${JOE_COLOR_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/joe/colors")

file(GLOB JOE_LANG_FILES "${PROJECT_SOURCE_DIR}/po/*.po")
install(FILES ${JOE_LANG_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/joe/lang")

install(FILES "${PROJECT_BINARY_DIR}/charmaps/klingon"
	DESTINATION "${CMAKE_INSTALL_DATADIR}/joe/charmaps")

install(FILES README.md docs/README.old docs/man.md ChangeLog docs/hacking.md NEWS.md
	DESTINATION "${CMAKE_INSTALL_DOCDIR}")

install(FILES "${PROJECT_BINARY_DIR}/man/joe.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
install(FILES "${PROJECT_BINARY_DIR}/man/ru/joe.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/ru/man1")

install(FILES desktop/joe.desktop desktop/jmacs.desktop desktop/jstar.desktop desktop/jpico.desktop
	DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
