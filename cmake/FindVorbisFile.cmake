# Locate VorbisFile
# This module defines XXX_FOUND, XXX_INCLUDE_DIRS and XXX_LIBRARIES standard variables
#
# $VORBISDIR is an environment variable that would
# correspond to the ./configure --prefix=$VORBISDIR
# used in building Vorbis.

# Copied from 
# http://code.google.com/p/osgaudio/source/browse/trunk/CMakeModules/FindVorbisFile.cmake

SET(VORBISFILE_SEARCH_PATHS
	~/Library/Frameworks
	/Library/Frameworks
    /usr/local
	/usr
	/sw # Fink
	/opt/local # DarwinPorts
	/opt/csw # Blastwave
	/opt
)

SET(MSVC_YEAR_NAME)
IF (MSVC_VERSION GREATER 1599)		# >= 1600
	SET(MSVC_YEAR_NAME VS2010)
ELSEIF(MSVC_VERSION GREATER 1499)	# >= 1500
	SET(MSVC_YEAR_NAME VS2008)
ELSEIF(MSVC_VERSION GREATER 1399)	# >= 1400
	SET(MSVC_YEAR_NAME VS2005)
ELSEIF(MSVC_VERSION GREATER 1299)	# >= 1300
	SET(MSVC_YEAR_NAME VS2003)
ELSEIF(MSVC_VERSION GREATER 1199)	# >= 1200
	SET(MSVC_YEAR_NAME VS6)
ENDIF()

FIND_PATH(VORBISFILE_INCLUDE_DIR
	NAMES vorbis/vorbisfile.h
	HINTS
	$ENV{VORBISFILEDIR}
	$ENV{VORBISFILE_PATH}
	$ENV{VORBISDIR}
	$ENV{VORBIS_PATH}
	PATH_SUFFIXES include
	PATHS ${VORBISFILE_SEARCH_PATHS}
)

FIND_LIBRARY(VORBISFILE_LIBRARY 
	NAMES vorbisfile libvorbisfile
	HINTS
	$ENV{VORBISFILEDIR}
	$ENV{VORBISFILE_PATH}
	$ENV{VORBISDIR}
	$ENV{VORBIS_PATH}
	PATH_SUFFIXES lib lib64 win32/VorbisFile_Dynamic_Release "Win32/${MSVC_YEAR_NAME}/x64/Release" "Win32/${MSVC_YEAR_NAME}/Win32/Release"
	PATHS ${VORBISFILE_SEARCH_PATHS}
)

# First search for d-suffixed libs
FIND_LIBRARY(VORBISFILE_LIBRARY_DEBUG 
	NAMES vorbisfiled vorbisfile_d libvorbisfiled libvorbisfile_d
	HINTS
	$ENV{VORBISFILEDIR}
	$ENV{VORBISFILE_PATH}
	$ENV{VORBISDIR}
	$ENV{VORBIS_PATH}
	PATH_SUFFIXES lib lib64 win32/VorbisFile_Dynamic_Debug "Win32/${MSVC_YEAR_NAME}/x64/Debug" "Win32/${MSVC_YEAR_NAME}/Win32/Debug"
	PATHS ${VORBISFILE_SEARCH_PATHS}
)

IF(NOT VORBISFILE_LIBRARY_DEBUG)
	# Then search for non suffixed libs if necessary, but only in debug dirs
	FIND_LIBRARY(VORBISFILE_LIBRARY_DEBUG 
		NAMES vorbisfile libvorbisfile
		HINTS
		$ENV{VORBISFILEDIR}
		$ENV{VORBISFILE_PATH}
		$ENV{VORBISDIR}
		$ENV{VORBIS_PATH}
		PATH_SUFFIXES win32/VorbisFile_Dynamic_Debug "Win32/${MSVC_YEAR_NAME}/x64/Debug" "Win32/${MSVC_YEAR_NAME}/Win32/Debug"
		PATHS ${VORBISFILE_SEARCH_PATHS}
	)
ENDIF()


IF(VORBISFILE_LIBRARY)
	IF(VORBISFILE_LIBRARY_DEBUG)
		SET(VORBISFILE_LIBRARIES optimized "${VORBISFILE_LIBRARY}" debug "${VORBISFILE_LIBRARY_DEBUG}")
	ELSE()
		SET(VORBISFILE_LIBRARIES "${VORBISFILE_LIBRARY}")		# Could add "general" keyword, but it is optional
	ENDIF()
ENDIF()

# handle the QUIETLY and REQUIRED arguments and set XXX_FOUND to TRUE if all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(VORBISFILE DEFAULT_MSG VORBISFILE_LIBRARIES VORBISFILE_INCLUDE_DIR)
