SET(NAME start)
FILE(GLOB_RECURSE FILELIST *.cpp *.hpp *.c *.h)
FOREACH(var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
	IF(${var} MATCHES "/MD")
		#STRING(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
	ENDIF()
ENDFOREACH()
LIST(LENGTH FILELIST LENGTH)
IF(LENGTH GREATER 0)
	ADD_LIBRARY(${NAME} SHARED EXCLUDE_FROM_ALL ${FILELIST})
	SET_TARGET_PROPERTIES(${NAME} PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/main.def")
	SET(LIBS minizip)
	LIST(LENGTH LIBS LENGTH)
	IF(LENGTH GREATER 0)
		TARGET_LINK_LIBRARIES(${NAME} ${LIBS})
	ENDIF()
ENDIF()