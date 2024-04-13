cmake_minimum_required(VERSION 3.1)

if(CMAKE_VERSION VERSION_GREATER 3.21)
	cmake_policy(SET CMP0128 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER 3.15)
	cmake_policy(SET CMP0091 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER 3.1)
	cmake_policy(SET CMP0054 NEW)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE INTERNAL "limit build types" FORCE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "default build type")

set(CMAKE_INSTALL_SO_NO_EXE False)

project(libaribb25 VERSION 0.2.9 LANGUAGES C CXX)
enable_language("CXX")

include(GitRevision)
include(GenerateExportHeader)
include(GNUInstallDirs)
find_package(PCSC REQUIRED)

if(UNIX OR MSYS)
	find_program(LDCONFIG_EXECUTABLE "ldconfig")
	set(WITH_PCSC_PACKAGE "libpcsclite" CACHE STRING "pkg-config package name for PC/SC")
	set(WITH_PCSC_LIBRARY "" CACHE STRING "library name for PC/SC")
endif()

if(WIN32)
	option(USE_UNICODE "enable unicode support" ON)
endif()
option(USE_AVX2 "enable AVX2" OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(ARM|ARM64|AARCH64)")
	option(USE_NEON "enable NEON" OFF)
endif()

# ---------- set variable (libaribb1) ----------

set(ARIBB1_LIB_NAME "aribb1")
set(ARIBB1_CMD_NAME "b1")
set(ARIBB1_STREAM_TEST_NAME "arib-b1-stream-test")

set(ARIBB1_URL "https://github.com/tsukumijima/libaribb25")
set(ARIBB1_DESCRIPTION "Reference implementation of ARIB STD-B1")
set(ARIBB1_COMPANY "㋲製作所")
set(ARIBB1_COPYRIGHT "©2012 茂木 和洋")
set(ARIBB1_PRODUCT "ARIB STD-B1")

set(ARIBB1_MAJOR_VERSION "0")
set(ARIBB1_MINOR_VERSION "2")
set(ARIBB1_PATCH_VERSION "9")
set(ARIBB1_VERSION_NUMBER ${ARIBB1_MAJOR_VERSION}.${ARIBB1_MINOR_VERSION}.${ARIBB1_PATCH_VERSION})
set(ARIBB1_VERSION_STRING ${ARIBB1_VERSION_NUMBER})

# ---------- set variable (libaribb25) ----------

set(ARIBB25_LIB_NAME "aribb25")
set(ARIBB25_CMD_NAME "b25")
set(ARIBB25_STREAM_TEST_NAME "arib-b25-stream-test")

set(ARIBB25_URL "https://github.com/tsukumijima/libaribb25")
set(ARIBB25_DESCRIPTION "Reference implementation of ARIB STD-B25")
set(ARIBB25_COMPANY "㋲製作所")
set(ARIBB25_COPYRIGHT "©2012 茂木 和洋")
set(ARIBB25_PRODUCT "ARIB STD-B25")

set(ARIBB25_MAJOR_VERSION "0")
set(ARIBB25_MINOR_VERSION "2")
set(ARIBB25_PATCH_VERSION "9")
set(ARIBB25_VERSION_NUMBER ${ARIBB25_MAJOR_VERSION}.${ARIBB25_MINOR_VERSION}.${ARIBB25_PATCH_VERSION})
set(ARIBB25_VERSION_STRING ${ARIBB25_VERSION_NUMBER})

# ---------- define flags ----------

if(CMAKE_C_COMPILER_ID MATCHES "(GNU|Clang)")
	set(CMAKE_C_FLAGS "-Wall")
	set(CMAKE_C_FLAGS_DEBUG "-O2 -g")
	set(CMAKE_C_FLAGS_RELEASE "-O2")
	set(CMAKE_CXX_FLAGS "-Wall")
	set(CMAKE_CXX_FLAGS_DEBUG "-O3 -g")
	set(CMAKE_CXX_FLAGS_RELEASE "-O3")
	set(CMAKE_SHARED_LINKER_FLAGS "-fvisibility=hidden")

	if(UNIX AND NOT CYGWIN)
		include(ElfInterp)
		set(CMAKE_C_FLAGS   "-fPIC ${CMAKE_C_FLAGS}")
		set(CMAKE_CXX_FLAGS "-fPIC ${CMAKE_CXX_FLAGS}")
		if(NOT APPLE)
			set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-eshow_version")
		endif()
	else(MINGW)
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-stdcall-fixup -static-libgcc")
	endif()

	if(MINGW AND USE_UNICODE)
		set(CMAKE_C_FLAGS   "-municode ${CMAKE_C_FLAGS}")
		set(CMAKE_CXX_FLAGS "-municode ${CMAKE_CXX_FLAGS}")
	endif()

	if(USE_AVX2)
		set(CMAKE_C_FLAGS   "-mavx2 ${CMAKE_C_FLAGS}")
		set(CMAKE_CXX_FLAGS "-mavx2 ${CMAKE_CXX_FLAGS}")
	endif()
	if(USE_NEON)
		set(CMAKE_C_FLAGS   "-mfpu=neon ${CMAKE_C_FLAGS}")
		set(CMAKE_CXX_FLAGS "-mfpu=neon ${CMAKE_CXX_FLAGS}")
	endif()
elseif(CMAKE_C_COMPILER_ID MATCHES "MSVC")
	add_definitions("-D_CRT_SECURE_NO_WARNINGS")
	set(CMAKE_SUPPRESS_REGENERATION TRUE)
	add_compile_options("/utf-8")

	if(USE_AVX2)
		add_definitions("-DENABLE_MULTI2_SIMD")
		set(CMAKE_C_FLAGS   "/arch:AVX2 ${CMAKE_C_FLAGS}")
		set(CMAKE_CXX_FLAGS "/arch:AVX2 ${CMAKE_CXX_FLAGS}")
	endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions("-DDEBUG")
endif()

if(USE_UNICODE)
	add_definitions("-DUNICODE" "-D_UNICODE")
endif()

add_definitions("-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64")
include_directories(${CMAKE_CURRENT_BINARY_DIR})
if(PCSC_INCLUDE_DIRS)
	include_directories(${PCSC_INCLUDE_DIRS})
endif()
link_directories(${PCSC_LIBRARY_DIRS})

# ---------- libaribb1 ----------

if(WIN32 AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "(ARM|ARM64|AARCH64)")
	add_library(aribb1-objlib OBJECT aribb25/arib_std_b25.c aribb25/b_cas_card.c aribb25/multi2.c aribb25/multi2_simd.c aribb25/ts_section_parser.c aribb25/version_b1.c)
else()
	add_library(aribb1-objlib OBJECT aribb25/arib_std_b25.c aribb25/b_cas_card.c aribb25/multi2.cc aribb25/ts_section_parser.c aribb25/version_b1.c)
endif()
set_target_properties(aribb1-objlib PROPERTIES COMPILE_DEFINITIONS ARIBB1_DLL)
target_compile_definitions(aribb1-objlib PRIVATE ENABLE_ARIB_STD_B1)

add_library(aribb1-static STATIC $<TARGET_OBJECTS:aribb1-objlib>)
set_target_properties(aribb1-static PROPERTIES OUTPUT_NAME ${ARIBB1_LIB_NAME})
target_link_libraries(aribb1-static PRIVATE ${PCSC_LIBRARIES})
target_compile_definitions(aribb1-static PRIVATE ENABLE_ARIB_STD_B1)

add_library(aribb1-shared SHARED $<TARGET_OBJECTS:aribb1-objlib> ${CMAKE_CURRENT_BINARY_DIR}/version_b1.rc)
set_target_properties(aribb1-shared PROPERTIES MACOSX_RPATH ON)
set_target_properties(aribb1-shared PROPERTIES DEFINE_SYMBOL ARIBB1_DLL)
set_target_properties(aribb1-shared PROPERTIES COMPILE_DEFINITIONS ARIBB1_DLL)
set_target_properties(aribb1-shared PROPERTIES OUTPUT_NAME ${ARIBB1_LIB_NAME})
set_target_properties(aribb1-shared PROPERTIES SOVERSION ${ARIBB1_MAJOR_VERSION})
set_target_properties(aribb1-shared PROPERTIES VERSION ${ARIBB1_VERSION_NUMBER})
target_link_libraries(aribb1-shared PRIVATE ${PCSC_LIBRARIES})
target_compile_definitions(aribb1-shared PRIVATE ENABLE_ARIB_STD_B1)

# ---------- b1 (executable) ----------

add_executable(b1 aribb25/td.c ${CMAKE_CURRENT_BINARY_DIR}/version_b1.rc)
set_target_properties(b1 PROPERTIES OUTPUT_NAME ${ARIBB1_CMD_NAME})
target_link_libraries(b1 PRIVATE ${PCSC_LIBRARIES})
target_link_libraries(b1 PRIVATE aribb1-shared)
target_compile_definitions(b1 PRIVATE ENABLE_ARIB_STD_B1)

configure_file(aribb25/config.h.in config.h @ONLY)
configure_file(aribb25/version_b1.rc.in version_b1.rc @ONLY)

# ---------- arib-b1-stream-test ----------

add_executable(arib-b1-stream-test aribb25/td.c ${CMAKE_CURRENT_BINARY_DIR}/version_b1.rc)
set_target_properties(arib-b1-stream-test PROPERTIES OUTPUT_NAME ${ARIBB1_STREAM_TEST_NAME})
target_link_libraries(arib-b1-stream-test PRIVATE ${PCSC_LIBRARIES})
target_link_libraries(arib-b1-stream-test PRIVATE aribb1-shared)
target_compile_definitions(arib-b1-stream-test PRIVATE ENABLE_ARIB_STD_B1)
target_compile_definitions(arib-b1-stream-test PRIVATE ENABLE_ARIB_STREAM_TEST)

configure_file(aribb25/config.h.in config.h @ONLY)
configure_file(aribb25/version_b1.rc.in version_b1.rc @ONLY)

# ---------- libaribb25 ----------

if(WIN32 AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "(ARM|ARM64|AARCH64)")
	add_library(aribb25-objlib OBJECT aribb25/arib_std_b25.c aribb25/b_cas_card.c aribb25/multi2.c aribb25/multi2_simd.c aribb25/ts_section_parser.c aribb25/version_b25.c)
else()
	add_library(aribb25-objlib OBJECT aribb25/arib_std_b25.c aribb25/b_cas_card.c aribb25/multi2.cc aribb25/ts_section_parser.c aribb25/version_b25.c)
endif()
set_target_properties(aribb25-objlib PROPERTIES COMPILE_DEFINITIONS ARIBB25_DLL)

add_library(aribb25-static STATIC $<TARGET_OBJECTS:aribb25-objlib>)
set_target_properties(aribb25-static PROPERTIES OUTPUT_NAME ${ARIBB25_LIB_NAME})
target_link_libraries(aribb25-static PRIVATE ${PCSC_LIBRARIES})

add_library(aribb25-shared SHARED $<TARGET_OBJECTS:aribb25-objlib> ${CMAKE_CURRENT_BINARY_DIR}/version_b25.rc)
set_target_properties(aribb25-shared PROPERTIES MACOSX_RPATH ON)
set_target_properties(aribb25-shared PROPERTIES DEFINE_SYMBOL ARIBB25_DLL)
set_target_properties(aribb25-shared PROPERTIES COMPILE_DEFINITIONS ARIBB25_DLL)
set_target_properties(aribb25-shared PROPERTIES OUTPUT_NAME ${ARIBB25_LIB_NAME})
set_target_properties(aribb25-shared PROPERTIES SOVERSION ${ARIBB25_MAJOR_VERSION})
set_target_properties(aribb25-shared PROPERTIES VERSION ${ARIBB25_VERSION_NUMBER})
target_link_libraries(aribb25-shared PRIVATE ${PCSC_LIBRARIES})

# ---------- b25 (executable) ----------

add_executable(b25 aribb25/td.c ${CMAKE_CURRENT_BINARY_DIR}/version_b25.rc)
set_target_properties(b25 PROPERTIES OUTPUT_NAME ${ARIBB25_CMD_NAME})
target_link_libraries(b25 PRIVATE ${PCSC_LIBRARIES})
target_link_libraries(b25 PRIVATE aribb25-shared)

configure_file(aribb25/config.h.in config.h @ONLY)
configure_file(aribb25/version_b25.rc.in version_b25.rc @ONLY)

# ---------- arib-b25-stream-test ----------

add_executable(arib-b25-stream-test aribb25/td.c ${CMAKE_CURRENT_BINARY_DIR}/version_b25.rc)
set_target_properties(arib-b25-stream-test PROPERTIES OUTPUT_NAME ${ARIBB25_STREAM_TEST_NAME})
target_link_libraries(arib-b25-stream-test PRIVATE ${PCSC_LIBRARIES})
target_link_libraries(arib-b25-stream-test PRIVATE aribb25-shared)
target_compile_definitions(arib-b25-stream-test PRIVATE ENABLE_ARIB_STREAM_TEST)

configure_file(aribb25/config.h.in config.h @ONLY)
configure_file(aribb25/version_b25.rc.in version_b25.rc @ONLY)

# ---------- install (Unix) ----------

if(UNIX AND NOT CYGWIN)

	add_custom_target(create_symlinks ALL)

	# ---------- libaribb1 ----------

	configure_file(aribb25/libaribb1.pc.in ${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB1_LIB_NAME}.pc @ONLY)
	add_custom_command(TARGET create_symlinks COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB1_LIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX} symlink-${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB1_LIB_NAME})
	add_custom_command(TARGET create_symlinks COMMAND ${CMAKE_COMMAND} -E create_symlink aribb1 symlink-${ARIBB1_LIB_NAME})
	add_dependencies(aribb1-static create_symlinks)
	add_dependencies(aribb1-shared create_symlinks)

	install(TARGETS b1 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(TARGETS arib-b1-stream-test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(TARGETS aribb1-static aribb1-shared ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
	install(DIRECTORY DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aribb1)
	install(FILES aribb25/arib_std_b25_error_code.h aribb25/arib_std_b25.h aribb25/b_cas_card_error_code.h aribb25/b_cas_card.h aribb25/multi2.h aribb25/portable.h aribb25/simd_instruction_type.h aribb25/ts_common_types.h aribb25/ts_section_parser_error_code.h aribb25/ts_section_parser.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aribb1)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB1_LIB_NAME}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/symlink-${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB1_LIB_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} RENAME ${CMAKE_SHARED_LIBRARY_PREFIX}arib1${CMAKE_SHARED_LIBRARY_SUFFIX})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/symlink-${ARIBB1_LIB_NAME} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} RENAME arib1)
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -DLDCONFIG_EXECUTABLE=${LDCONFIG_EXECUTABLE} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PostInstall.cmake)")

	# ---------- libaribb25 ----------

	configure_file(aribb25/libaribb25.pc.in ${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB25_LIB_NAME}.pc @ONLY)
	add_custom_command(TARGET create_symlinks COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB25_LIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX} symlink-${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB25_LIB_NAME})
	add_custom_command(TARGET create_symlinks COMMAND ${CMAKE_COMMAND} -E create_symlink aribb25 symlink-${ARIBB25_LIB_NAME})
	add_dependencies(aribb25-static create_symlinks)
	add_dependencies(aribb25-shared create_symlinks)

	install(TARGETS b25 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(TARGETS arib-b25-stream-test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(TARGETS aribb25-static aribb25-shared ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
	install(DIRECTORY DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aribb25)
	install(FILES aribb25/arib_std_b25_error_code.h aribb25/arib_std_b25.h aribb25/b_cas_card_error_code.h aribb25/b_cas_card.h aribb25/multi2.h aribb25/portable.h aribb25/simd_instruction_type.h aribb25/ts_common_types.h aribb25/ts_section_parser_error_code.h aribb25/ts_section_parser.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aribb25)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB25_LIB_NAME}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/symlink-${CMAKE_SHARED_LIBRARY_PREFIX}${ARIBB25_LIB_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} RENAME ${CMAKE_SHARED_LIBRARY_PREFIX}arib25${CMAKE_SHARED_LIBRARY_SUFFIX})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/symlink-${ARIBB25_LIB_NAME} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} RENAME arib25)
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -DLDCONFIG_EXECUTABLE=${LDCONFIG_EXECUTABLE} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PostInstall.cmake)")

	configure_file(aribb25/${ARIBB25_LIB_NAME}.pc.in ${ARIBB25_LIB_NAME}.pc @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${ARIBB25_LIB_NAME}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

	add_custom_target(uninstall ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Uninstall.cmake)

# ---------- install (Windows) ----------

elseif(WIN32)

	# ---------- libaribb1 ----------

	install(TARGETS b1 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(TARGETS aribb1-static aribb1-shared ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR})
	install(FILES aribb25/arib_std_b25_error_code.h aribb25/arib_std_b25.h aribb25/b_cas_card_error_code.h aribb25/b_cas_card.h aribb25/multi2.h aribb25/portable.h aribb25/simd_instruction_type.h aribb25/ts_common_types.h aribb25/ts_section_parser_error_code.h aribb25/ts_section_parser.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aribb1)

	# ---------- libaribb25 ----------

	install(TARGETS b25 RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(TARGETS aribb25-static aribb25-shared ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR})
	install(FILES aribb25/arib_std_b25_error_code.h aribb25/arib_std_b25.h aribb25/b_cas_card_error_code.h aribb25/b_cas_card.h aribb25/multi2.h aribb25/portable.h aribb25/simd_instruction_type.h aribb25/ts_common_types.h aribb25/ts_section_parser_error_code.h aribb25/ts_section_parser.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aribb25)

	add_custom_target(uninstall ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Uninstall.cmake)

endif()

# ---------- CPack ----------

# CPack を使用して Debian パッケージをビルドする際は、常に `cmake -B build -DCMAKE_INSTALL_PREFIX=/usr` と実行しなければならない
# CMAKE_INSTALL_PREFIX はデフォルトで /usr/local に設定されているが、何らかの理由でライブラリを /usr/lib に配置する Debian パッケージが生成されてしまう
# この結果、pkg-config 設定ファイルの記述内容と齟齬が発生し、外部ソフトウェアがライブラリを正しくリンクできなくなる

if(UNIX AND NOT CYGWIN AND NOT APPLE)
	set(CPACK_GENERATOR "DEB")
	set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ARIB STD-B1 / ARIB STD-B25 library")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
	set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
	set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")  # ref: https://stackoverflow.com/a/59202791/17124142
	set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpcsclite1 (>= 1.8.23)")
	SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "tsukumi <tsukumijima@users.noreply.github.com>")
	set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
	include(CPack)
endif()
