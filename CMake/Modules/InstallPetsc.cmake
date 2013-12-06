#InstallPetsc.cmake
#
# Created on: Jul 20, 2012
#     Author: jb
#
# accepted variables: 
#     EXTERNAL_PETSC_DIR - directory used for instalation
#
#     PETSC_MPI_DIR      - pass MPI to PETSC configure 
#     PETSC_LAPACK_DIR   - pass Lapack to PETSC configure
#
#     PETSC_CONFIG - possible values:
#       mini             - only petsc + necessary MPI, BLAS, LAPACK
#       flow123d_mini    - mini + metis + parmetis
#       bddcml           - flow123d_mini + mumps + scalapack + blacs
#       full             - bddcml + hypre + blopex + umfpack + sundials
#       default)         = flow123d_mini
#
#     INSTALL_PETSC_OPTIONS - additional options used as parameters to configure.py,
#
# We automatically reuse compiler and their flags. Further we set "--with-debugging=1" 
# if FLOW_BUILD_TYPE == "debug", otherwise we turn debugging off.
#
# CAUTION: Never use semicolon a part of compiler options or INSTALL_PETSC_OPTIONS.


if (NOT EXTERNAL_PETSC_DIR)
    set(EXTERNAL_PETSC_DIR "${EXTERNAL_PROJECT_DIR}/petsc_build")
endif()    

##########################################################################
# Download PETSC
# A temporary CMakeLists.txt

# set compilers
set(PETSC_CONF_LINE --CC=${CMAKE_C_COMPILER} --CFLAGS='${CMAKE_C_FLAGS}' --CXX=${CMAKE_CXX_COMPILER} --CXXFLAGS='${CMAKE_CXX_FLAGS}' --with-clanguage=C)
if (CMAKE_Fortran_COMPILER)
    set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --FC=${CMAKE_Fortran_COMPILER} --FFLAGS=${CMAKE_Fortran_FLAGS})
endif()

# set debugging
if (FLOW_BUILD_TYPE STREQUAL "debug")
  set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --with-debugging=1)
else()
  set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --with-debugging=0)
endif()

# set necessary libraries: MPI, BLAS, LAPACK
if (PETSC_MPI_DIR)
    set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --with-mpi-dir=${PETSC_MPI_DIR})
else()
    set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --download-mpich=yes)
endif()

if (PETSC_LAPACK_DIR)
    set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --with-lapack-dir=${PETSC_LAPACK_DIR})
else()
    if(CMAKE_Fortran_COMPILER)
        set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --download-f-blas-lapack=yes)
    else()
        set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --download-c-blas-lapack=yes)
    endif()    
endif()

# set configuration
if (NOT PETSC_CONFIG OR PETSC_CONFIG STREQUAL "default")
  set(PETSC_CONFIG "flow123d_mini")
endif()

if(PETSC_CONFIG STREQUAL "mini")
    # nothing to add
elseif(PETSC_CONFIG STREQUAL "flow123d_mini")
    set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --download-metis=yes --download-parmetis=yes)
elseif(PETSC_CONFIG STREQUAL "bddcml")
    set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --download-metis=yes --download-parmetis=yes --download-blacs=yes --download-scalapack=yes --download-mumps=yes)
elseif(PETSC_CONFIG STREQUAL "full")
    if (CMAKE_HOST_WIN32 OR WIN32 OR CYGWIN) # CMAKE_HOST_WIN32 should include win32, win64, and cygwin; but doesn't work
        set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --download-metis=yes --download-parmetis=yes  --download-blacs=yes --download-scalapack=yes --download-mumps=yes )
    else()    
        set(PETSC_CONF_LINE ${PETSC_CONF_LINE} --download-metis=yes --download-parmetis=yes --download-hypre=yes --download-blacs=yes --download-scalapack=yes --download-mumps=yes --download-blopex=yes --download-umfpack=yes --download-sundials=yes)
    endif()
else()
    message("Unknown value of PETSC_CONFIG. Using 'mini'.")
endif()    

# additional options
set(PETSC_CONF_LINE ${PETSC_CONF_LINE} ${INSTALL_PETSC_OPTIONS})
string(REPLACE ";" " " EXPAND_CONF_LINE "${PETSC_CONF_LINE}")



# create CMakeLists.txt for external project
set (cmakelists_fname "${EXTERNAL_PETSC_DIR}/CMakeLists.txt")
file (WRITE "${cmakelists_fname}"
"
  ## This file was autogenerated by InstallPETSC.cmake
  cmake_minimum_required(VERSION 2.8)
  include(ExternalProject)
  ExternalProject_Add(PETSC
    DOWNLOAD_DIR ${EXTERNAL_PETSC_DIR} 
    URL \"http://bacula.nti.tul.cz/~jan.brezina/flow123d_libraries/petsc-lite-3.4.3.tar.gz\"
    SOURCE_DIR ${EXTERNAL_PETSC_DIR}/src
    BINARY_DIR ${EXTERNAL_PETSC_DIR}/src
    CONFIGURE_COMMAND bash ${EXTERNAL_PETSC_DIR}/conf.sh
    BUILD_COMMAND make all
    INSTALL_COMMAND \"\"
  )  
")

# create configuration script (overcome troubles with two expansion levels)
file(WRITE "${EXTERNAL_PETSC_DIR}/conf_tmp.sh"
"
cd ${EXTERNAL_PETSC_DIR}/src
./configure ${EXPAND_CONF_LINE}
")

# avoid unnecessary rebuilds if configuration doesn't change
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${EXTERNAL_PETSC_DIR}/conf_tmp.sh" "${EXTERNAL_PETSC_DIR}/conf.sh")



message(STATUS "=== Installing PETSC ===")
# run cmake
set(PETSC_DIR "${EXTERNAL_PETSC_DIR}/src")
set(PETSC_ARCH "flow123d_release")
set(ENV{PETSC_DIR} "${PETSC_DIR}")
set(ENV{PETSC_ARCH} "${PETSC_ARCH}")
execute_process(COMMAND ${CMAKE_COMMAND} ${EXTERNAL_PETSC_DIR} 
      WORKING_DIRECTORY ${EXTERNAL_PETSC_DIR})

find_program (MAKE_EXECUTABLE NAMES make gmake)
# run make
execute_process(COMMAND ${MAKE_EXECUTABLE} VERBOSE=1 PETSC
      WORKING_DIRECTORY ${EXTERNAL_PETSC_DIR})    


#file (REMOVE ${cmakelists_fname})

message(STATUS "== PETSC build done")

