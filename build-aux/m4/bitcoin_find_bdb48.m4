dnl Copyright (c) 2013-2015 The BurritoCoin Core developers
dnl Distributed under the MIT software license, see the accompanying
dnl file COPYING or http://www.opensource.org/licenses/mit-license.php.

dnl Preferred BDB version: 5.3 (Ubuntu libdb++-dev). Also accepts 4.8.
AC_DEFUN([BURRITOCOIN_FIND_BDB48],[
  AC_ARG_VAR(BDB_CFLAGS, [C compiler flags for BerkeleyDB, bypasses autodetection])
  AC_ARG_VAR(BDB_LIBS, [Linker flags for BerkeleyDB, bypasses autodetection])

  if test "x$BDB_CFLAGS" = "x"; then
    AC_MSG_CHECKING([for Berkeley DB C++ headers (5.3 or 4.8)])
    BDB_CPPFLAGS=
    bdbpath=X
    bdb48path=X
    bdb53path=X
    bdbdirlist=
    for _vn in 5.3 53 5 4.8 48 4 ''; do
      for _pfx in b lib ''; do
        bdbdirlist="$bdbdirlist ${_pfx}db${_vn}"
      done
    done
    for searchpath in $bdbdirlist ''; do
      test -n "${searchpath}" && searchpath="${searchpath}/"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
        #include <${searchpath}db_cxx.h>
      ]],[[
        #if !((DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 8) || DB_VERSION_MAJOR > 4)
          #error "failed to find bdb 4.8+"
        #endif
      ]])],[
        if test "x$bdbpath" = "xX"; then
          bdbpath="${searchpath}"
        fi
      ],[
        continue
      ])
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
        #include <${searchpath}db_cxx.h>
      ]],[[
        #if !(DB_VERSION_MAJOR == 5 && DB_VERSION_MINOR == 3)
          #error "not bdb 5.3"
        #endif
      ]])],[
        if test "x$bdb53path" = "xX"; then
          bdb53path="${searchpath}"
          break
        fi
      ],[])
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
        #include <${searchpath}db_cxx.h>
      ]],[[
        #if !(DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 8)
          #error "not bdb 4.8"
        #endif
      ]])],[
        if test "x$bdb48path" = "xX"; then
          bdb48path="${searchpath}"
        fi
      ],[])
    done
    if test "x$bdbpath" = "xX"; then
      AC_MSG_RESULT([no])
      AC_MSG_ERROR([libdb_cxx headers missing. ]AC_PACKAGE_NAME[ requires BerkeleyDB 5.3 (Ubuntu: apt install libdb++-dev) or 4.8 for wallet functionality (--disable-wallet to disable)])
    elif test "x$bdb53path" != "xX"; then
      dnl BDB 5.3 is the preferred version — no warning needed.
      BURRITOCOIN_SUBDIR_TO_INCLUDE(BDB_CPPFLAGS,[${bdb53path}],db_cxx)
      bdbpath="${bdb53path}"
      AC_MSG_RESULT([yes (5.3)])
    elif test "x$bdb48path" != "xX"; then
      BURRITOCOIN_SUBDIR_TO_INCLUDE(BDB_CPPFLAGS,[${bdb48path}],db_cxx)
      bdbpath="${bdb48path}"
      AC_MSG_RESULT([yes (4.8)])
    else
      dnl Found something other than 4.8 or 5.3.
      BURRITOCOIN_SUBDIR_TO_INCLUDE(BDB_CPPFLAGS,[${bdbpath}],db_cxx)
      AC_ARG_WITH([incompatible-bdb],[AS_HELP_STRING([--with-incompatible-bdb], [allow using a bdb version other than 4.8 or 5.3])],[
        AC_MSG_WARN([Found Berkeley DB other than 4.8 or 5.3; wallets may not be portable!])
      ],[
        AC_MSG_ERROR([Found Berkeley DB other than 4.8 or 5.3. Use --with-incompatible-bdb to override, or --disable-wallet to disable wallet functionality.])
      ])
      AC_MSG_RESULT([yes (non-standard)])
    fi
  else
    BDB_CPPFLAGS=${BDB_CFLAGS}
  fi
  AC_SUBST(BDB_CPPFLAGS)

  if test "x$BDB_LIBS" = "x"; then
    for searchlib in db_cxx db_cxx-5.3 db_cxx-4.8 db4_cxx; do
      AC_CHECK_LIB([$searchlib],[main],[
        BDB_LIBS="-l${searchlib}"
        break
      ])
    done
    if test "x$BDB_LIBS" = "x"; then
        AC_MSG_ERROR([libdb_cxx missing. ]AC_PACKAGE_NAME[ requires BerkeleyDB 5.3 (Ubuntu: apt install libdb++-dev) for wallet functionality (--disable-wallet to disable)])
    fi
  fi
  AC_SUBST(BDB_LIBS)
])
