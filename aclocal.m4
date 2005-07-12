dnl These modifications are to allow for an empty cross compiler tree.
dnl In the situation that cross-linking is impossible, the variable
dnl `cross_linkable' will be substituted with "yes".

AC_DEFUN([hurd_SYSTYPE],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
case "$host_cpu" in
i[[3456]]86)	systype=i386 ;;
*)		systype="$host_cpu" ;;
esac
AC_SUBST([systype])])
