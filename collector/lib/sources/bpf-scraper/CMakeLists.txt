
bpf_object(program_iter bpf/programs.bpf.c)

get_target_property(SKEL_SOURCES program_iter_skel INTERFACE_SOURCES)

add_library(collector_scrapers
    BPFProgramIterator.cpp
    BPFProgramIterator.h
    BPFSignalFormatter.h
    ${SKEL_SOURCES}
)

target_include_directories(collector_scrapers PRIVATE $<TARGET_PROPERTY:rox-proto,INCLUDE_DIRECTORIES>)
target_include_directories(collector_scrapers PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(collector_scrapers rox-proto)
