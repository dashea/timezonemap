#!/bin/sh

# This has the most confusing failure case if missing (HAVE_INTROSPECTION does not appear in AM_CONDITIONAL), so check for it up front
if ! pkg-config gobject-introspection-1.0 ; then
    echo "gobject-introspection development files are required"
    exit 1
fi

autoreconf -i
