# Source files
set(XO_CORE_SOURCES
    markdown.c
    template.c
    build.c
    server.c
    utils.c
    watcher.c
)

# Create library
add_library(xo_core STATIC ${XO_CORE_SOURCES})

# Platform specific configurations
if(WIN32)
    # Add Windows-specific preprocessor definitions
    target_compile_definitions(xo_core PRIVATE 
        _CRT_SECURE_NO_WARNINGS  # Disable warnings about "unsafe" functions
        WIN32_LEAN_AND_MEAN      # Exclude rarely-used stuff from Windows headers
    )
    
    # Link with Windows-specific libraries
    target_link_libraries(xo_core ws2_32)
else()
    # On non-Windows platforms, link with math and pthread libraries
    target_link_libraries(xo_core m pthread)
endif()

# Link dependencies
target_link_libraries(xo_core 
    # Add any additional libraries here
) 