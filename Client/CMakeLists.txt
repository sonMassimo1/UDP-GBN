add_executable(client client.c)

add_custom_command(TARGET client PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_CURRENT_SOURCE_DIR}/files $<TARGET_FILE_DIR:client>/files)

