cmake_minimum_required(VERSION 3.12)

project(jirachi C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)


if(ANDROID)
    message(STATUS "Building for Android (API ${ANDROID_PLATFORM})")
    set(BUILDING_FOR_ANDROID ON)
else()
    set(BUILDING_FOR_ANDROID OFF)
endif()


option(JNIHOOK_BUILD_SHARED "Build shared library" ON)
option(JNIHOOK_BUILD_STATIC "Build static library" ON)
option(JNIHOOK_BUILD_TESTS "Build test examples" OFF)
option(JNIHOOK_RUN_TESTS_AUTO "Automatically run tests after build" ON)


if(NOT ANDROID)
    set(JAVA_HOME $ENV{JAVA_HOME} CACHE STRING "Java home directory")
    if(NOT JAVA_HOME)
        message(FATAL_ERROR "JAVA_HOME not set. Please set JAVA_HOME environment variable.")
    endif()

    set(JAVA_INCLUDES "${JAVA_HOME}/include")
    if(WIN32)
        list(APPEND JAVA_INCLUDES "${JAVA_HOME}/include/win32")
    elseif(APPLE)
        list(APPEND JAVA_INCLUDES "${JAVA_HOME}/include/darwin")
    else()
        list(APPEND JAVA_INCLUDES "${JAVA_HOME}/include/linux")
    endif()
else()
    set(JAVA_INCLUDES "")
endif()


set(JNIHOOK_SOURCES
        src/jnihook.c
        src/hashtable.c
        src/buffer.c
        src/classfile.c
        src/mutex.c
        src/hook_manager.c
)

if(ANDROID)
    list(APPEND JNIHOOK_SOURCES src/android_compat.c)
endif()

set(JNIHOOK_HEADERS
        include/jnihook.h
        include/hashtable.h
        include/buffer.h
        include/classfile.h
        include/mutex.h
        include/hook_manager.h
        include/internal.h
)

if(ANDROID)
    list(APPEND JNIHOOK_HEADERS include/android_compat.h)
endif()


if(JNIHOOK_BUILD_SHARED)
    add_library(jirachi SHARED ${JNIHOOK_SOURCES})

    target_include_directories(jirachi
            PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
            ${JAVA_INCLUDES}
    )

    if(WIN32)
        target_compile_definitions(jirachi PRIVATE JNIHOOK_EXPORTS)
    else()
        set_target_properties(jirachi PROPERTIES
                C_VISIBILITY_PRESET hidden
                VISIBILITY_INLINES_HIDDEN ON
        )
    endif()

    if(ANDROID)
        target_compile_definitions(jirachi PUBLIC JNIHOOK_ANDROID)
        target_link_libraries(jirachi PRIVATE log)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(jirachi PRIVATE pthread)
    endif()
endif()

if(JNIHOOK_BUILD_SHARED)
    add_library(jnihook ALIAS jirachi)
endif()


if(JNIHOOK_BUILD_STATIC)
    add_library(jirachi_static STATIC ${JNIHOOK_SOURCES})

    target_include_directories(jirachi_static
            PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
            ${JAVA_INCLUDES}
    )

    if(ANDROID)
        target_compile_definitions(jirachi_static PUBLIC JNIHOOK_ANDROID)
        target_link_libraries(jirachi_static PUBLIC log)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(jirachi_static PUBLIC pthread)
    endif()

    if(WIN32)
        set_target_properties(jirachi_static PROPERTIES OUTPUT_NAME jirachi_static)
    else()
        set_target_properties(jirachi_static PROPERTIES OUTPUT_NAME jirachi)
    endif()
endif()

if(JNIHOOK_BUILD_STATIC)
    add_library(jnihook_static ALIAS jirachi_static)
endif()



if(MSVC)
    if(JNIHOOK_BUILD_SHARED)
        target_compile_options(jirachi PRIVATE /W4 /WX /wd4100 /wd4996)
    endif()
    if(JNIHOOK_BUILD_STATIC)
        target_compile_options(jirachi_static PRIVATE /W4 /WX /wd4100 /wd4996)
    endif()
else()
    if(JNIHOOK_BUILD_SHARED)
        target_compile_options(jirachi PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    if(JNIHOOK_BUILD_STATIC)
        target_compile_options(jirachi_static PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()


if(JNIHOOK_BUILD_TESTS AND NOT ANDROID)
    add_subdirectory(tests)

    if(JNIHOOK_RUN_TESTS_AUTO)
        add_custom_target(auto_run_tests ALL
                COMMAND ${CMAKE_COMMAND} -E echo ""
                COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
                COMMAND ${CMAKE_COMMAND} -E echo "Running JNIHook Tests"
                COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
                COMMAND ${CMAKE_COMMAND} -E echo ""
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_test
                DEPENDS test_hook test_java_class
                COMMENT "Running tests after build"
        )
    endif()
endif()


if(JNIHOOK_BUILD_SHARED)
    install(TARGETS jirachi
            EXPORT jirachi-targets
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            RUNTIME DESTINATION bin
    )
endif()

if(JNIHOOK_BUILD_STATIC)
    install(TARGETS jirachi_static
            EXPORT jirachi-targets
            ARCHIVE DESTINATION lib
    )
endif()

install(FILES ${JNIHOOK_HEADERS} DESTINATION include/jirachi)

install(EXPORT jirachi-targets
        FILE jirachi-config.cmake
        NAMESPACE jirachi::
        DESTINATION lib/cmake/jirachi
)
