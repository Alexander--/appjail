#!/bin/sh

aclocal -I m4 --install
autoheader
automake --copy --add-missing
autoconf --warnings=error
