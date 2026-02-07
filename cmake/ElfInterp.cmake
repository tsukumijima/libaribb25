
find_program(READELF_EXECUTABLE "readelf")
if(READELF_EXECUTABLE)
	set(ELF_INTERP_BIN ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/elf_interp)
	set(ELF_INTERP_SRC ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/elf_interp.c)
	set(ELF_INTERP_DAT ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/elf_interp.dat)

	file(WRITE ${ELF_INTERP_SRC} "int main(int argc, char **argv) { return 0; }")

	execute_process(COMMAND ${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS} -o ${ELF_INTERP_BIN} ${ELF_INTERP_SRC})
	execute_process(COMMAND ${READELF_EXECUTABLE} -l ${ELF_INTERP_BIN} OUTPUT_FILE ${ELF_INTERP_DAT})

	if(EXISTS ${ELF_INTERP_DAT})
		file(READ ${ELF_INTERP_DAT} ELF_INTERP_READELF)
		string(REGEX MATCH "interpreter\: (.*)(\\].*)" _ "${ELF_INTERP_READELF}")
		string(STRIP ${CMAKE_MATCH_1} ELF_INTERP)
	endif()
endif()

message(STATUS "ELF Interpreter: ${ELF_INTERP}")
