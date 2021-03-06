# 
# Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
#
# Please make a following refer to Flow123d on your project site if you use the program for any purpose,
# especially for academic research:
# Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
#
# This program is free software; you can redistribute it and/or modify it under the terms
# of the GNU General Public License version 3 as published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program; if not,
# write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
#
# $Id: CMakeLists.txt 1306 2011-08-30 13:53:56Z jiri.jenicek $
# $Revision: 1306 $
# $LastChangedBy: jiri.jenicek $
# $LastChangedDate: 2011-08-30 15:53:56 +0200 (Tue, 30 Aug 2011) $
#

# This file runs original Makefile shipped with TBB and in addition creates static libraries.
# It is however strongly recommended to use shared libraries, so we should swith to them
# as soon as possible.

message(STATUS "CMakeLists.txt - TBB")

find_program(MAKE_EXECUTABLE NAMES make gmake)
find_program(AR_EXECUTABLE NAMES ar)

set(TBB_DIR "${CMAKE_CURRENT_SOURCE_DIR}" PARENT_SCOPE)
set(TBB_LIBRARY_DIR "${PROJECT_BINARY_DIR}/lib")
set(TBB_LIBRARIES "${TBB_LIBRARY_DIR}/libtbb.a"
                  "${TBB_LIBRARY_DIR}/libtbbmalloc.a"
)
set(TBB_LIBRARIES ${TBB_LIBRARIES} PARENT_SCOPE)
set(TBB_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include/" PARENT_SCOPE)

# create temporary makefile for printing the variables
set(tbb_config_makefile "${PROJECT_BINARY_DIR}/Makefile.tbb")
file (WRITE "${tbb_config_makefile}"
"## This file was autogenerated by CMake (TBB).
tbb_root=${CMAKE_CURRENT_SOURCE_DIR}
include ${CMAKE_CURRENT_SOURCE_DIR}/build/Makefile.tbb
include ${CMAKE_CURRENT_SOURCE_DIR}/build/Makefile.tbbmalloc
include ${CMAKE_CURRENT_SOURCE_DIR}/build/Makefile.tbbproxy
show :
\t-@echo -n \${\${VARIABLE}}
")
# macro for extraction of variables from makefile
macro(TBB_GET_VARIABLE name var)
  set (${var} "NOTFOUND" CACHE INTERNAL "Cleared" FORCE)
  execute_process (COMMAND ${MAKE_EXECUTABLE} --no-print-directory -f ${tbb_config_makefile} show VARIABLE=${name}
    OUTPUT_VARIABLE ${var}
    RESULT_VARIABLE petsc_return)
endmacro(TBB_GET_VARIABLE)


# prepend directory where objects will be located
macro(PREPEND_DIRECTORY_NAME files dir)
  string(REPLACE " " ";" file_list ${${files}})
  set(${files} "" CACHE INTERNAL "Cleared" FORCE)
  foreach(obj ${file_list})
    set(${files} ${${files}} "${dir}/${obj}")
  endforeach()
endmacro(PREPEND_DIRECTORY_NAME)

# save object files for TBB libraries
TBB_GET_VARIABLE(TBB.OBJ tbb_obj)
TBB_GET_VARIABLE(MALLOC.OBJ tbb_malloc_obj)
TBB_GET_VARIABLE(TBB.RES tbb_res)
TBB_GET_VARIABLE(MALLOC.RES tbb_malloc_res)
#TBB_GET_VARIABLE(PROXY.OBJ tbb_proxy_obj)
prepend_directory_name(tbb_obj "${CMAKE_CURRENT_BINARY_DIR}/build_${FLOW_BUILD_TYPE}")
prepend_directory_name(tbb_malloc_obj "${CMAKE_CURRENT_BINARY_DIR}/build_${FLOW_BUILD_TYPE}")
#prepend_directory_name(tbb_proxy_obj "${CMAKE_CURRENT_BINARY_DIR}/build_${FLOW_BUILD_TYPE}")

# finally remove the temporary makefile
file(REMOVE "${tbb_config_makefile}")


# add tager for TBB
# run make
add_custom_command(OUTPUT ${TBB_LIBRARIES}
       COMMAND "${MAKE_EXECUTABLE}" tbb_build_dir=${CMAKE_CURRENT_BINARY_DIR} tbb_build_prefix=build VERBOSE=1
       COMMAND ${AR_EXECUTABLE} cr ${TBB_LIBRARY_DIR}/libtbb.a ${tbb_obj} ${tbb_res}
       COMMAND "${AR_EXECUTABLE}" cr ${TBB_LIBRARY_DIR}/libtbbmalloc.a ${tbb_malloc_obj} ${tbb_malloc_res}
#       COMMAND "${AR_EXECUTABLE}" cr ${TBB_LIBRARY_DIR}/libtbbmalloc_proxy.a ${tbb_proxy_obj}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      )

add_custom_target(tbb SOURCES ${TBB_LIBRARIES})
