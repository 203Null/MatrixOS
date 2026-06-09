# Influence Application
file(GLOB_RECURSE INFLUENCE_SOURCES "*.cpp" "*.c")
file(GLOB_RECURSE INFLUENCE_HEADERS "*.h")

add_library(InfluenceAPP ${INFLUENCE_SOURCES} ${INFLUENCE_HEADERS})

target_include_directories(InfluenceAPP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(InfluenceAPP PUBLIC
    MatrixOSInterface
    MatrixOSAPPInterface
)

# Register the application class
RegisterApplicationClass(Influence)
