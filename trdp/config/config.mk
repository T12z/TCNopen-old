#//
#// $Id$
#//
#// DESCRIPTION    Config file to make TRDP for POSIX_X86 target
#//
#// AUTHOR         NewTec GmbH
#//
#// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0 
#// If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/
#// Copyright NewTec GmbH, 2017. All rights reserved.
#//

ARCH = linux-x86_64
TARGET_VOS = posix
TARGET_OS = LINUX
TCPREFIX = 
TCPOSTFIX = 
DOXYPATH = 

# the _GNU_SOURCE is needed to get the extended poll feature for the POSIX socket

CFLAGS += -Wall -m64 -fstrength-reduce -fno-builtin -fsigned-char -pthread -fPIC -D_GNU_SOURCE -DPOSIX -DL_ENDIAN
LDFLAGS += -lrt

LINT_SYSINCLUDE_DIRECTIVES = -i ./src/vos/posix -wlib 0 -DL_ENDIAN
