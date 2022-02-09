project(fat16 C)

set(CMAKE_C_STANDARD 11)

add_executable(fat16 main.c options.c fat16.c io.c io.h utils.c)

target_link_libraries(fat16 -lfuse3)