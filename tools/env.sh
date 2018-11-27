# This file is part of the Home2L project.
#
# (C) 2015-2018 Gundolf Kiefer


# HOME2L_ROOT ...
export HOME2L_ROOT=`realpath \`dirname $BASH_SOURCE\``

# Architecture (for arch-dependent paths)...
export HOME2L_ARCH=`dpkg --print-architecture`

# Search and library paths...
export PATH=$HOME2L_ROOT/etc:$HOME2L_ROOT/bin:$HOME2L_ROOT/bin/$HOME2L_ARCH:$PATH
export LD_LIBRARY_PATH=$HOME2L_ROOT/lib/$HOME2L_ARCH:$LD_LIBRARY_PATH
export PYTHONPATH=$HOME2L_ROOT/lib/$HOME2L_ARCH:$PYTHONPATH
