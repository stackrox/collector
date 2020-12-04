#!/usr/bin/env bash

set -e


wget "https://github.com/ThomasDickey/ncurses-snapshots/archive/v${NCURSES_VERSION}.tar.gz"
tar -xzf "v${NCURSES_VERSION}.tar.gz"
cd "ncurses-snapshots-${NCURSES_VERSION}"
cp AUTHORS "${LICENSE_DIR}/ncurses-${NCURSES_VERSION}"
./configure --enable-overwrite --prefix="/usr/local" --with-curses-h --without-cxx --without-cxx-binding --without-ada --without-manpages --without-progs --without-tests --with-terminfo-dirs=/etc/terminfo:/lib/terminfo:/usr/share/terminfo
make -j "${NPROCS:-2}"
make install
