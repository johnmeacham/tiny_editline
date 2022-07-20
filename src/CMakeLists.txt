if (NOT TARGET tiny_editline)
	add_library(tiny_editline INTERFACE)
	target_sources(tiny_editline INTERFACE
		${CMAKE_CURRENT_LIST_DIR}/editline.c
		)
	target_include_directories(tiny_editline INTERFACE ${CMAKE_CURRENT_LIST_DIR})
endif()
