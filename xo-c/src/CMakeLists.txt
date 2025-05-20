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

# Link dependencies
target_link_libraries(xo_core 
    # Add any additional libraries here
)

# Platform specific libraries
if(WIN32)
    target_link_libraries(xo_core ws2_32)
else()
    target_link_libraries(xo_core m)
endif() 