function(add_cxx_files TARGET)
	file(GLOB_RECURSE INCLUDE_FILES
		LIST_DIRECTORIES false
		CONFIGURE_DEPENDS
		"include/*.h"
		"include/*.hpp"
		"include/*.hxx"
		"include/*.inl"
	)

	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include
		PREFIX "Header Files"
		FILES ${INCLUDE_FILES})

	target_sources("${TARGET}" PUBLIC ${INCLUDE_FILES})

	file(GLOB_RECURSE HEADER_FILES
		LIST_DIRECTORIES false
		CONFIGURE_DEPENDS
		"src/*.h"
		"src/*.hpp"
		"src/*.hxx"
		"src/*.inl"
	)

	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
		PREFIX "Header Files"
		FILES ${HEADER_FILES})

	target_sources("${TARGET}" PRIVATE ${HEADER_FILES})

	file(GLOB_RECURSE SOURCE_FILES
		LIST_DIRECTORIES false
		CONFIGURE_DEPENDS
		"src/*.cpp"
		"src/*.cxx"
	)

	source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src
		PREFIX "Source Files"
		FILES ${SOURCE_FILES})

	target_sources("${TARGET}" PRIVATE ${SOURCE_FILES})
endfunction()
