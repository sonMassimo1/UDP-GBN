add_executable(server server.c)

add_custom_command(TARGET server PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_CURRENT_SOURCE_DIR}/files $<TARGET_FILE_DIR:server>/files)

