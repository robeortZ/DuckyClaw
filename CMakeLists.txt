##
# @file CMakeLists.txt
# @brief 
#/

# APP_PATH
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRCS
aux_source_directory(${APP_PATH}/src APP_SRCS)
aux_source_directory(${APP_PATH}/tools APP_SRCS)
aux_source_directory(${APP_PATH}/cron_service APP_SRCS)
aux_source_directory(${APP_PATH}/todo_service APP_SRCS)
aux_source_directory(${APP_PATH}/heartbeat APP_SRCS)
aux_source_directory(${APP_PATH}/memory APP_SRCS)
aux_source_directory(${APP_PATH}/gateway APP_SRCS)
aux_source_directory(${APP_PATH}/skills APP_SRCS)
aux_source_directory(${APP_PATH}/agent APP_SRCS)
aux_source_directory(${APP_PATH}/display APP_SRCS)
aux_source_directory(${APP_PATH}/display/image APP_SRCS)
aux_source_directory(${APP_PATH}/dev_config APP_SRCS)
# APP_INC
set(APP_INC
    ${APP_PATH}
    ${APP_PATH}/src
    ${APP_PATH}/include
    ${APP_PATH}/tools
    ${APP_PATH}/cron_service
    ${APP_PATH}/todo_service
    ${APP_PATH}/heartbeat
    ${APP_PATH}/memory
    ${APP_PATH}/gateway
    ${APP_PATH}/skills
    ${APP_PATH}/agent
    ${APP_PATH}/ai_components/assets/include
    ${APP_PATH}/display
    ${APP_PATH}/dev_config
)

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRCS}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

add_subdirectory(${APP_PATH}/ai_components ${CMAKE_BINARY_DIR}/ai_components)
add_subdirectory(${APP_PATH}/IM ${CMAKE_BINARY_DIR}/IM)