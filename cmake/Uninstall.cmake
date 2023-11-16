set(MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

if(NOT EXISTS ${MANIFEST})
	message(FATAL_ERROR "Cannot find install manifest: '${MANIFEST}'")
endif()

file(STRINGS ${MANIFEST} files)
set(dirs "")

foreach(file ${files})
	if(EXISTS ${file} OR IS_SYMLINK ${file})
		message(STATUS "Removing file: '${file}'")

		exec_program(
			${CMAKE_COMMAND} ARGS "-E remove ${file}"
			OUTPUT_VARIABLE stdout
			RETURN_VALUE result
		)

		if(NOT "${result}" STREQUAL 0)
			message(FATAL_ERROR "Failed to remove file: '${file}'.")
		endif()

		# Get the directory of the file and add it to the list
		get_filename_component(dir ${file} DIRECTORY)
		set(dirs ${dirs} ${dir})
	endif()
endforeach(file)

# Remove empty directories
list(REMOVE_DUPLICATES dirs)
list(SORT dirs)
list(REVERSE dirs) # Start from the deepest directory

foreach(dir ${dirs})
	if(IS_DIRECTORY ${dir})
		file(GLOB children RELATIVE ${dir} ${dir}/*)

		if(NOT children)
			message(STATUS "Removing empty directory: '${dir}'")
			exec_program(
				${CMAKE_COMMAND} ARGS "-E remove_directory ${dir}"
				OUTPUT_VARIABLE stdout
				RETURN_VALUE result
			)

			if(NOT "${result}" STREQUAL 0)
				message(FATAL_ERROR "Failed to remove directory: '${dir}'.")
			endif()
		endif()
	endif()
endforeach(dir)
