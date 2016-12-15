include_directories("${JSONCPP_INCLUDE}")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

if(NOT WIN32)
    if(NOT APPLE)
        include_directories("${CURL_INCLUDE_DIR}")
    endif()
	include_directories("${CURSES_INCLUDE_DIR}")
endif()

include_directories("${PROJECT_SOURCE_DIR}/common")
include_directories("${PROJECT_SOURCE_DIR}/userspace/libscap")
include_directories("${PROJECT_SOURCE_DIR}/userspace/libsinsp")
include_directories("${PROJECT_BINARY_DIR}/userspace/sysdig")
include_directories("/usr/include/librdkafka")
include_directories(.)

if(NOT WIN32)
	set(SOURCE_FILES
		fields_info.cpp
		KafkaClient.cpp
		sysdig.cpp)

	set(SOURCE_FILES_CSYSDIG
		fields_info.cpp
		csysdig.cpp)
else()
	set(SOURCE_FILES
		fields_info.cpp
		sysdig.cpp
		win32/getopt.c)

	set(SOURCE_FILES_CSYSDIG
		fields_info.cpp
		csysdig.cpp
		win32/getopt.c)
endif()

add_executable(sysdig ${SOURCE_FILES})
add_library(sysdignative SHARED ${SOURCE_FILES})
add_executable(csysdig ${SOURCE_FILES_CSYSDIG})
add_library(rdkafka STATIC IMPORTED GLOBAL)
#set_target_properties(rdkafka PROPERTIES IMPORTED_LOCATION "/usr/lib/x86_64-linux-gnu/librdkafka.a")
find_library(RDKAFKA NAMES librdkafka.a)

SET(NATIVELIBS
  sinsp
  ${RDKAFKA}
)

if(NOT WIN32)

	target_link_libraries(sysdig
		${RDKAFKA} sinsp)

	target_link_libraries(sysdignative
		-Wl,--start-group ${NATIVELIBS} -Wl,--end-group)

	if(USE_BUNDLED_NCURSES)
		add_dependencies(csysdig ncurses)
	endif()

	target_link_libraries(csysdig
		sinsp
		"${CURSES_LIBRARIES}")

	add_subdirectory(man)

	install(TARGETS sysdig 
		DESTINATION bin)

	install(TARGETS sysdignative
		DESTINATION bin)

	install(TARGETS csysdig 
		DESTINATION bin)

	install(DIRECTORY chisels
		DESTINATION share/sysdig)

	file(COPY chisels
		DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

else()

	target_link_libraries(sysdig
		sinsp)

	target_link_libraries(csysdig
		sinsp)

	target_link_libraries(sysdig odbc32.lib odbccp32.lib)

	target_link_libraries(csysdig odbc32.lib odbccp32.lib)

	add_custom_command(TARGET sysdig POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${LUAJIT_SRC}/lua51.dll"
			"${PROJECT_BINARY_DIR}/$(Configuration)/lua51.dll")

#	add_custom_command(TARGET sysdig POST_BUILD
#		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
#			"${ZLIB_INCLUDE}/zlib1.dll"
#			"${PROJECT_BINARY_DIR}/$(Configuration)/")

	add_custom_command(TARGET sysdig POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_directory
			"${PROJECT_SOURCE_DIR}/userspace/sysdig/chisels"
			"${PROJECT_BINARY_DIR}/$(Configuration)/chisels")

	add_custom_command(TARGET sysdig POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			$<TARGET_FILE:sysdig>
			"${PROJECT_BINARY_DIR}/$(Configuration)/sysdig.exe")

	add_custom_command(TARGET csysdig POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			$<TARGET_FILE:csysdig>
			"${PROJECT_BINARY_DIR}/$(Configuration)/csysdig.exe")

endif()

configure_file(config_sysdig.h.in config_sysdig.h)
