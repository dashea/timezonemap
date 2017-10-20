#!/bin/sh

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.

which gnome-autogen.sh || {
	echo "You need gnome-common from GNOME Git"
	exit 1
}

. gnome-autogen.sh $@
