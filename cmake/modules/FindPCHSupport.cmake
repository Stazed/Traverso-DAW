# - Try to find precompiled headers support for GCC 3.4 and 4.x
# Once done this will define:
#
# Variable:
#   PCHSupport_FOUND
#
# Macro:
#   ADD_PRECOMPILED_HEADER

IF(CMAKE_COMPILER_IS_GNUCXX)
    EXEC_PROGRAM(${CMAKE_CXX_COMPILER}
                 ARGS -dumpversion
                 OUTPUT_VARIABLE gcc_compiler_version)
#   STRING(REGEX REPLACE ".* ([0-9]\\.[0-9]\\.[0-9]) .*" "\\1" 
#          gcc_compiler_version ${_compiler_output})
#   MESSAGE("GCC Version: ${gcc_compiler_version}")
    IF(gcc_compiler_version MATCHES "4\\.[0-9]\\.[0-9]")
        SET(PCHSupport_FOUND TRUE)
    ELSE(gcc_compiler_version MATCHES "4\\.[0-9]\\.[0-9]")
        IF(gcc_compiler_version MATCHES "3\\.4\\.[0-9]")
            SET(PCHSupport_FOUND TRUE)
        ENDIF(gcc_compiler_version MATCHES "3\\.4\\.[0-9]")
    ENDIF(gcc_compiler_version MATCHES "4\\.[0-9]\\.[0-9]")
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

MACRO(ADD_PRECOMPILED_HEADER _targetName _input)
    GET_FILENAME_COMPONENT(_name ${_input} NAME)
    SET(_source "${CMAKE_CURRENT_SOURCE_DIR}/${_input}")
    SET(_outdir "${CMAKE_CURRENT_BINARY_DIR}/${_name}.gch")
    MAKE_DIRECTORY(${_outdir})
    SET(_output "${_outdir}/${CMAKE_BUILD_TYPE}.c++")
    STRING(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" _flags_var_name)
    SET(_compiler_FLAGS ${${_flags_var_name}})
    SEPARATE_ARGUMENTS(_compiler_FLAGS)
    #MESSAGE("_compiler_FLAGS: ${_compiler_FLAGS}")
    ADD_CUSTOM_COMMAND(
        OUTPUT ${_output}
        COMMAND ${CMAKE_CXX_COMPILER}
           ${_compiler_FLAGS}
           -I${QT_INCLUDE_DIR}
           -I${QT_QTCORE_INCLUDE_DIR}
	   -I${QT_QTGUI_INCLUDE_DIR}
	   -I${QT_QTXML_INCLUDE_DIR}
	   -I${CMAKE_SOURCE_DIR}/src/core
	   -I${CMAKE_SOURCE_DIR}/src/common
	   -I${CMAKE_SOURCE_DIR}/src/sheetcanvas
           ${QT_DEFINITIONS}
           ${TRAVERSO_DEFINES}
           -x c++-header
           -o ${_output} ${_source}
        DEPENDS ${_source} )
    ADD_CUSTOM_TARGET(${_targetName} DEPENDS ${_output})
ENDMACRO(ADD_PRECOMPILED_HEADER)
