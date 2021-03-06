include("ezUtilsVars")
include("ezUtilsPCH")
include("ezUtilsUnityFiles")
include("ezUtilsQt")
include("ezUtilsDetect")
include("ezUtilsDX11")
include("ezUtilsNuGet")
include("ezUtilsAssImp")
include("ezUtilsRmlUi")
include("ezUtilsCI")
include("ezUtilsCppFlags")
include("ezUtilsAndroid")
include("ezUtilsUWP")
include("ezUtilsTarget")
include("ezUtilsTargetCS")
include("ezUtilsVcpkg")
include("ezUtilsSubmodule")

######################################
### ez_set_target_output_dirs(<target> <lib-output-dir> <dll-output-dir>)
######################################

function(ez_set_target_output_dirs TARGET_NAME LIB_OUTPUT_DIR DLL_OUTPUT_DIR)

	ez_pull_all_vars()

	set(SUB_DIR "")

	if (EZ_CMAKE_PLATFORM_WINDOWS_UWP)
		# UWP has deployment problems if all applications output to the same path.
		set(SUB_DIR "/${TARGET_NAME}")
	endif()

	set (OUTPUT_LIB_DEBUG       "${LIB_OUTPUT_DIR}/${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}Debug${EZ_CMAKE_ARCHITECTURE_POSTFIX}${SUB_DIR}")
	set (OUTPUT_LIB_RELEASE     "${LIB_OUTPUT_DIR}/${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}Release${EZ_CMAKE_ARCHITECTURE_POSTFIX}${SUB_DIR}")
	set (OUTPUT_LIB_MINSIZE     "${LIB_OUTPUT_DIR}/${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}MinSize${EZ_CMAKE_ARCHITECTURE_POSTFIX}${SUB_DIR}")
	set (OUTPUT_LIB_RELWITHDEB  "${LIB_OUTPUT_DIR}/${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}RelDeb${EZ_CMAKE_ARCHITECTURE_POSTFIX}${SUB_DIR}")

	set (OUTPUT_DLL_DEBUG       "${DLL_OUTPUT_DIR}/${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}Debug${EZ_CMAKE_ARCHITECTURE_POSTFIX}${SUB_DIR}")
	set (OUTPUT_DLL_RELEASE     "${DLL_OUTPUT_DIR}/${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}Release${EZ_CMAKE_ARCHITECTURE_POSTFIX}${SUB_DIR}")
	set (OUTPUT_DLL_MINSIZE     "${DLL_OUTPUT_DIR}/${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}MinSize${EZ_CMAKE_ARCHITECTURE_POSTFIX}${SUB_DIR}")
	set (OUTPUT_DLL_RELWITHDEB  "${DLL_OUTPUT_DIR}/${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}RelDeb${EZ_CMAKE_ARCHITECTURE_POSTFIX}${SUB_DIR}")

	# If we can't use generator expressions the non-generator expression version of the
	# output directory should point to the version matching CMAKE_BUILD_TYPE. This is the case for
	# add_custom_command BYPRODUCTS for example needed by Ninja.
	if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		set_target_properties(${TARGET_NAME} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DLL_DEBUG}"
			LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_LIB_DEBUG}"
			ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_LIB_DEBUG}"
		)
	elseif (${CMAKE_BUILD_TYPE} STREQUAL "Release")
		set_target_properties(${TARGET_NAME} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DLL_RELEASE}"
			LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_LIB_RELEASE}"
			ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_LIB_RELEASE}"
		)
	elseif (${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
		set_target_properties(${TARGET_NAME} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DLL_MINSIZE}"
			LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_LIB_MINSIZE}"
			ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_LIB_MINSIZE}"
		)
	elseif (${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
		set_target_properties(${TARGET_NAME} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DLL_RELWITHDEB}"
			LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_LIB_RELWITHDEB}"
			ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_LIB_RELWITHDEB}"
		)
	endif()	

	set_target_properties(${TARGET_NAME} PROPERTIES
	  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DLL_DEBUG}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_LIB_DEBUG}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_LIB_DEBUG}"
  )

	set_target_properties(${TARGET_NAME} PROPERTIES
	  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DLL_RELEASE}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_LIB_RELEASE}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_LIB_RELEASE}"
  )

	set_target_properties(${TARGET_NAME} PROPERTIES
	  RUNTIME_OUTPUT_DIRECTORYMINSIZEREL "${OUTPUT_DLL_MINSIZE}"
    LIBRARY_OUTPUT_DIRECTORYMINSIZEREL "${OUTPUT_LIB_MINSIZE}"
    ARCHIVE_OUTPUT_DIRECTORYMINSIZEREL "${OUTPUT_LIB_MINSIZE}"
	)

	set_target_properties(${TARGET_NAME} PROPERTIES
	  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_DLL_RELWITHDEB}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_LIB_RELWITHDEB}"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${OUTPUT_LIB_RELWITHDEB}"
	)

endfunction()

######################################
### ez_set_default_target_output_dirs(<target>)
######################################

function(ez_set_default_target_output_dirs TARGET_NAME)

	set (EZ_OUTPUT_DIRECTORY_LIB "${CMAKE_SOURCE_DIR}/Output/Lib" CACHE PATH "Where to store the compiled .lib files.")
	set (EZ_OUTPUT_DIRECTORY_DLL "${CMAKE_SOURCE_DIR}/Output/Bin" CACHE PATH "Where to store the compiled .dll files.")

	mark_as_advanced(FORCE EZ_OUTPUT_DIRECTORY_LIB)
	mark_as_advanced(FORCE EZ_OUTPUT_DIRECTORY_DLL)

	ez_set_target_output_dirs("${TARGET_NAME}" "${EZ_OUTPUT_DIRECTORY_LIB}" "${EZ_OUTPUT_DIRECTORY_DLL}")

endfunction()


######################################
### ez_write_configuration_txt()
######################################

function(ez_write_configuration_txt)

	# Clear Targets.txt and Tests.txt
	file(WRITE ${CMAKE_BINARY_DIR}/Targets.txt "")
	file(WRITE ${CMAKE_BINARY_DIR}/Tests.txt "")

	ez_pull_all_vars()

	# Write configuration to file, as this is done at configure time we must pin the configuration in place (RelDeb is used because all build machines use this).
	file(WRITE ${CMAKE_BINARY_DIR}/Configuration.txt "")
	set(CONFIGURATION_DESC "${EZ_CMAKE_PLATFORM_PREFIX}${EZ_CMAKE_GENERATOR_PREFIX}${EZ_CMAKE_COMPILER_POSTFIX}RelDeb${EZ_CMAKE_ARCHITECTURE_POSTFIX}")
	file(APPEND ${CMAKE_BINARY_DIR}/Configuration.txt ${CONFIGURATION_DESC})

endfunction()


######################################
### ez_add_target_folder_as_include_dir(<target> <path-to-target>)
######################################

function(ez_add_target_folder_as_include_dir TARGET_NAME TARGET_FOLDER)

	get_filename_component(PARENT_DIR ${TARGET_FOLDER} DIRECTORY)

	target_include_directories(${TARGET_NAME} PRIVATE "${TARGET_FOLDER}")
	target_include_directories(${TARGET_NAME} PUBLIC "${PARENT_DIR}")

endfunction()

######################################
### ez_set_common_target_definitions(<target>)
######################################

function(ez_set_common_target_definitions TARGET_NAME)

	ez_pull_all_vars()

	# set the BUILDSYSTEM_COMPILE_ENGINE_AS_DLL definition
	if (EZ_COMPILE_ENGINE_AS_DLL)
		target_compile_definitions(${TARGET_NAME} PUBLIC BUILDSYSTEM_COMPILE_ENGINE_AS_DLL)
	endif()

	# set the BUILDSYSTEM_CONFIGURATION definition
	target_compile_definitions(${TARGET_NAME} PRIVATE BUILDSYSTEM_CONFIGURATION="${EZ_CMAKE_GENERATOR_CONFIGURATION}")

	# set the BUILDSYSTEM_BUILDING_XYZ_LIB definition
	string(TOUPPER ${TARGET_NAME} PROJECT_NAME_UPPER)
	target_compile_definitions(${TARGET_NAME} PRIVATE BUILDSYSTEM_BUILDING_${PROJECT_NAME_UPPER}_LIB)

endfunction()

######################################
### ez_set_project_ide_folder(<target> <path-to-target>)
######################################

function(ez_set_project_ide_folder TARGET_NAME PROJECT_SOURCE_DIR)

	# globally enable sorting targets into folders in IDEs
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)

	get_filename_component (PARENT_FOLDER ${PROJECT_SOURCE_DIR} PATH)
	get_filename_component (FOLDER_NAME ${PARENT_FOLDER} NAME)

	set(IDE_FOLDER "${FOLDER_NAME}")

	if (${PROJECT_SOURCE_DIR} MATCHES "/Code/")

		get_filename_component (PARENT_FOLDER ${PARENT_FOLDER} PATH)
		get_filename_component (FOLDER_NAME ${PARENT_FOLDER} NAME)

		while(NOT ${FOLDER_NAME} STREQUAL "Code")

			set(IDE_FOLDER "${FOLDER_NAME}/${IDE_FOLDER}")

			get_filename_component (PARENT_FOLDER ${PARENT_FOLDER} PATH)
			get_filename_component (FOLDER_NAME ${PARENT_FOLDER} NAME)
		
		endwhile()

	endif()

	get_property(EZ_SUBMODULE_MODE GLOBAL PROPERTY EZ_SUBMODULE_MODE)
	
	if(EZ_SUBMODULE_MODE)
		set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER "ezEngine/${IDE_FOLDER}")
	else()
		set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER ${IDE_FOLDER})
	endif()

endfunction()

######################################
### ez_add_output_ez_prefix(<target>)
######################################

function(ez_add_output_ez_prefix TARGET_NAME)

	set_target_properties(${TARGET_NAME} PROPERTIES IMPORT_PREFIX "ez")
	set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "ez")

endfunction()

######################################
### ez_set_library_properties(<target>)
######################################

function(ez_set_library_properties TARGET_NAME)

	ez_pull_all_vars()

	if (EZ_CMAKE_PLATFORM_LINUX)
		# Workaround for: https://bugs.launchpad.net/ubuntu/+source/gcc-5/+bug/1568899
		target_link_libraries (${TARGET_NAME} PRIVATE -lgcc_s -lgcc pthread rt)
	endif ()

	if (EZ_CMAKE_PLATFORM_OSX OR EZ_CMAKE_PLATFORM_LINUX)
		find_package(X11 REQUIRED)
		find_package(SFML REQUIRED system window)
		target_include_directories (${TARGET_NAME} PRIVATE ${X11_X11_INCLUDE_PATH})
		target_link_libraries (${TARGET_NAME} PRIVATE ${X11_X11_LIB} sfml-window sfml-system)
	endif ()

endfunction()

######################################
### ez_set_application_properties(<target>)
######################################

function(ez_set_application_properties TARGET_NAME)

	ez_pull_all_vars()

	if (EZ_CMAKE_PLATFORM_OSX OR EZ_CMAKE_PLATFORM_LINUX)
		find_package(X11 REQUIRED)
		find_package(SFML REQUIRED system window)
		target_include_directories (${TARGET_NAME} PRIVATE ${X11_X11_INCLUDE_PATH})
		target_link_libraries (${TARGET_NAME} PRIVATE ${X11_X11_LIB} sfml-window sfml-system)
	endif ()

	# We need to link against X11, pthread and rt last or linker errors will occur.
	if (EZ_CMAKE_COMPILER_GCC)
		target_link_libraries (${TARGET_NAME} PRIVATE pthread rt)
	endif ()

endfunction()

######################################
### ez_set_natvis_file(<target> <path-to-natvis-file>)
######################################

function(ez_set_natvis_file TARGET_NAME NATVIS_FILE)

	# We need at least visual studio 2015 for this to work
	if ((MSVC_VERSION GREATER 1900) OR (MSVC_VERSION EQUAL 1900))

		target_sources(${TARGET_NAME} PRIVATE ${NATVIS_FILE})

	endif()

endfunction()


######################################
### ez_make_winmain_executable(<target>)
######################################

function(ez_make_winmain_executable TARGET_NAME)

	set_property(TARGET ${TARGET_NAME} PROPERTY WIN32_EXECUTABLE ON)

endfunction()


######################################
### ez_gather_subfolders(<abs-path-to-folder> <out-sub-folders>)
######################################

function(ez_gather_subfolders START_FOLDER RESULT_FOLDERS)

	set(ALL_FILES "")
	set(ALL_DIRS "")

	file(GLOB_RECURSE ALL_FILES RELATIVE "${START_FOLDER}" "${START_FOLDER}/*")

	foreach(FILE ${ALL_FILES})

		get_filename_component(FILE_PATH ${FILE} DIRECTORY)

		list(APPEND ALL_DIRS ${FILE_PATH})

	endforeach()

	list(REMOVE_DUPLICATES ALL_DIRS)

	set(${RESULT_FOLDERS} ${ALL_DIRS} PARENT_SCOPE)

endfunction()


######################################
### ez_glob_source_files(<path-to-folder> <out-files>)
######################################

function(ez_glob_source_files ROOT_DIR RESULT_ALL_SOURCES)

  file(GLOB_RECURSE CPP_FILES "${ROOT_DIR}/*.cpp" "${ROOT_DIR}/*.cc")
  file(GLOB_RECURSE H_FILES "${ROOT_DIR}/*.h" "${ROOT_DIR}/*.hpp" "${ROOT_DIR}/*.inl")
  file(GLOB_RECURSE C_FILES "${ROOT_DIR}/*.c")
  file(GLOB_RECURSE CS_FILES "${ROOT_DIR}/*.cs")

  file(GLOB_RECURSE UI_FILES "${ROOT_DIR}/*.ui")
  file(GLOB_RECURSE QRC_FILES "${ROOT_DIR}/*.qrc")
  file(GLOB_RECURSE RES_FILES "${ROOT_DIR}/*.ico" "${ROOT_DIR}/*.rc")

  set(${RESULT_ALL_SOURCES} ${CPP_FILES} ${H_FILES} ${C_FILES} ${CS_FILES} ${UI_FILES} ${QRC_FILES} ${RES_FILES} PARENT_SCOPE)

endfunction()

######################################
### ez_add_all_subdirs()
######################################

function(ez_add_all_subdirs)

	# find all cmake files below this directory
	file (GLOB SUB_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt")

	foreach (VAR ${SUB_DIRS})

		get_filename_component (RES ${VAR} DIRECTORY)

		add_subdirectory (${RES})

	endforeach ()

endfunction()

######################################
### ez_cmake_init()
######################################

macro(ez_cmake_init)

	ez_pull_all_vars()

endmacro()

######################################
### ez_requires(<variable>)
######################################

macro(ez_requires)

  if(${ARGC} EQUAL 0)
    return()
  endif()

  set(ALL_ARGS "${ARGN}")

  foreach(arg IN LISTS ALL_ARGS)

    if (NOT ${arg})
      return()
    endif()

  endforeach()

endmacro()

######################################
### ez_requires_windows()
######################################

macro(ez_requires_windows)

    ez_requires(EZ_CMAKE_PLATFORM_WINDOWS)

endmacro()

######################################
### ez_requires_editor()
######################################

macro(ez_requires_editor)

		ez_requires_qt()
		ez_requires_d3d()

endmacro()

######################################
### ez_add_external_folder(<project-number>)
######################################

function(ez_add_external_projects_folder PROJECT_NUMBER)

	set(CACHE_VAR_NAME "EZ_EXTERNAL_PROJECT${PROJECT_NUMBER}")

	set (${CACHE_VAR_NAME} "" CACHE PATH "A folder outside the ez repository that should be parsed for CMakeLists.txt files to include projects into the ez solution.")

	set(CACHE_VAR_VALUE ${${CACHE_VAR_NAME}})

	if (NOT CACHE_VAR_VALUE)
		return()
	endif()

	set_property(GLOBAL PROPERTY "GATHER_EXTERNAL_PROJECTS" TRUE)
	add_subdirectory(${CACHE_VAR_VALUE} "${CMAKE_BINARY_DIR}/ExternalProject${PROJECT_NUMBER}")
	set_property(GLOBAL PROPERTY "GATHER_EXTERNAL_PROJECTS" FALSE)

endfunction()

######################################
### ez_init_projects()
######################################

function(ez_init_projects)

	# find all init.cmake files below this directory
	file (GLOB_RECURSE INIT_FILES "init.cmake")

	foreach (INIT_FILE ${INIT_FILES})

		message(STATUS "Including '${INIT_FILE}'")
		include("${INIT_FILE}")

	endforeach ()
	
endfunction()

######################################
### ez_finalize_projects()
######################################

function(ez_finalize_projects)

	# find all init.cmake files below this directory
	file (GLOB_RECURSE INIT_FILES "finalize.cmake")

	foreach (INIT_FILE ${INIT_FILES})

		message(STATUS "Including '${INIT_FILE}'")
		include("${INIT_FILE}")

	endforeach ()
	
endfunction()

######################################
### ez_build_filter_init()
######################################

# The build filter is intended to only build a subset of ezEngine. 
# This is modeled as a enum so it is extensible
# use the ez_build_filter_xxx marcros to filter out libraries.
# Currently the only two values are
# Foundation - 0: only build foundation and related tests
# Everything - 1: build everything
function(ez_build_filter_init)

	set(EZ_BUILD_FILTER "Everything" CACHE STRING "Whether tool projects should be added to the solution")
	set(EZ_BUILD_FILTER_VALUES FoundationOnly Everything)
	set_property(CACHE EZ_BUILD_FILTER PROPERTY STRINGS ${EZ_BUILD_FILTER_VALUES})
	list(FIND EZ_BUILD_FILTER_VALUES ${EZ_BUILD_FILTER} index)
	set_property(GLOBAL PROPERTY EZ_BUILD_FILTER_INDEX ${index})
	
endfunction()

######################################
### ez_build_filter_foundation()
######################################

# Project will only be included if build filter is set to foundation or higher
macro(ez_build_filter_foundation)
	get_property(filterIndex GLOBAL PROPERTY EZ_BUILD_FILTER_INDEX)
	if(${filterIndex} LESS 0)
		return()
	endif()
endmacro()

######################################
### ez_build_filter_everything()
######################################

# Project will only be included if build filter is set to everything
macro(ez_build_filter_everything)
	get_property(filterIndex GLOBAL PROPERTY EZ_BUILD_FILTER_INDEX)
	if(${filterIndex} LESS 1)
		return()
	endif()
endmacro()