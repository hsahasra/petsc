#requirespackage  'PETSC_HAVE_AMS'

ALL: lib

CFLAGS   = -DAMS_HOME='"${AMS_HOME}"'
FFLAGS   =
SOURCEC  = ams.c amsopen.c
SOURCEF  =
SOURCEH  = ../../../../../../include/petscviewerams.h
MANSEC   = Viewer
LIBBASE  = libpetscsys
DIRS     =
LOCDIR   = src/sys/classes/viewer/impls/ams/

include ${PETSC_DIR}/conf/variables
include ${PETSC_DIR}/conf/rules
include ${PETSC_DIR}/conf/test




