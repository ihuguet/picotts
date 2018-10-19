#!/bin/sh

#created by aclocal
rm -rf autom4te.cache
rm -f aclocal.m4

#created by libtoolize
rm -rf m4
mkdir m4
rm -f ltmain.sh

#created by autoconf
rm -f configure

#created by automake
rm -f install-sh missing depcomp Makefile.in config.guess config.sub
rm -f INSTALL COPYING compile

#created by ./configure
rm -rf .deps
rm -f Makefile config.log config.status libtool

if [ "$1" = "clean" ]; then
    exit
fi

IPATHS="-I lib"

libtoolize --copy
aclocal $IPATHS
automake --add-missing --copy
autoconf $IPATHS

rm -rf autom4te.cache

echo "Now run ./configure and then make."
exit 0

