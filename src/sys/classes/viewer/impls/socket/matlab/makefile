
#
#requirespackage  'PETSC_HAVE_MATLAB'
#requiresscalar real
#requiresprecision  double
#
#   matlabcodes makes the MATLAB mex files for receiving PETSc objects
# in MATLAB. You cannot use a complex numbers.
#
SPECIALLIB     = yes
SPECIALFASTLIB = yes
speciallib:
	-@echo "BEGINNING TO COMPILE MATLAB INTERFACE"
	-@if [ ! -d "${PETSC_DIR}/bin/matlab" ] ; then ${MKDIR}  ${PETSC_DIR}/bin/matlab; fi
	-@${OMAKE}  matlabcodes PETSC_ARCH=${PETSC_ARCH} PETSC_DIR=${PETSC_DIR}
specialfastlib: speciallib

matlabcodes: sread sopen sclose swrite

CFLAGS    =
FFLAGS    =
CPPFLAGS  =
SOURCEC   =
SOURCEF   =
SOURCEH   = sopen.c sclose.c  bread.c sread.c swrite.c
MANSEC    = Viewer
LIBBASE   = libpetscsys
DIRS      =
# LOCDIR is set to 0 because the string version is not passed properly down to MATLAB's mex program
LOCDIR    = src/sys/classes/viewer/impls/socket/matlab/

include ${PETSC_DIR}/conf/variables
include ${PETSC_DIR}/conf/rules
include ${PETSC_DIR}/conf/test

sread:
	-@${MATLAB_MEX}  -g CC='${PCC}' CFLAGS='${COPTFLAGS} ${CC_FLAGS} ${CCPPFLAGS}' sread.c bread.c -lc
	-@${RM} -f sread.o bread.o
	-@${MV} sread.mex* ${PETSC_DIR}/bin/matlab

#  This is hardwired to one particular system
longdoubleread:
	${MATLAB_MEX}  -v -g CC='${PCC}'  CFLAGS='${COPTFLAGS} ${CC_FLAGS} ${CCPPFLAGS}' longdoubleread.c bread.c /usr/local/lib/libgcc_ext.10.4.dylib -lc
	-@${RM} -f longdoubleread.o bread.o
	-@${MV} longdoubleread.mex* ${PETSC_DIR}/bin/matlab

swrite:
	-@${MATLAB_MEX} -g CC='${PCC}' CFLAGS='${COPTFLAGS} ${CC_FLAGS} ${CCPPFLAGS}' swrite.c bread.c -lc
	-@${RM} -f swrite.o bread.o
	-@${MV} swrite.mex* ${PETSC_DIR}/bin/matlab

sopen:
	-@${MATLAB_MEX} -g CC='${PCC}' CFLAGS='${COPTFLAGS} ${CC_FLAGS} ${CCPPFLAGS} ${PCC_FLAGS}' sopen.c -lc
	-@${RM} -f sopen.o
	-@${MV} sopen.mex* ${PETSC_DIR}/bin/matlab

sclose:
	-@${MATLAB_MEX} -g CC='${PCC}' CFLAGS='${COPTFLAGS} ${CC_FLAGS} ${CCPPFLAGS} ${PCC_FLAGS}' sclose.c -lc
	-@${RM} -f sclose.o
	-@${MV} sclose.mex* ${PETSC_DIR}/bin/matlab

