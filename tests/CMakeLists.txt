
find_package(Java REQUIRED COMPONENTS Development)


set(TEST_JAVA_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/TestClass.java")
set(TEST_JAVA_CLASS "${CMAKE_CURRENT_BINARY_DIR}/TestClass.class")


add_custom_command(
        OUTPUT ${TEST_JAVA_CLASS}
        COMMAND ${Java_JAVAC_EXECUTABLE} -d ${CMAKE_CURRENT_BINARY_DIR} ${TEST_JAVA_SOURCE}
        DEPENDS ${TEST_JAVA_SOURCE}
        COMMENT "Compiling TestClass.java"
        VERBATIM
)

add_custom_target(test_java_class ALL DEPENDS ${TEST_JAVA_CLASS})


add_library(test_hook SHARED test_hook.c)
target_link_libraries(test_hook PRIVATE jnihook)
target_include_directories(test_hook PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${JAVA_INCLUDES}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(test_hook PRIVATE pthread dl)
endif()


set(AGENT_LIB_PATH $<TARGET_FILE:test_hook>)


if(WIN32)
    set(LIB_PATH_VAR "PATH")
    set(LIB_PATH_SEP ";")
elseif(APPLE)
    set(LIB_PATH_VAR "DYLD_LIBRARY_PATH")
    set(LIB_PATH_SEP ":")
else()
    set(LIB_PATH_VAR "LD_LIBRARY_PATH")
    set(LIB_PATH_SEP ":")
endif()


add_custom_target(run_test
        COMMAND ${CMAKE_COMMAND} -E env
        "${LIB_PATH_VAR}=$<TARGET_FILE_DIR:jnihook>${LIB_PATH_SEP}$ENV{${LIB_PATH_VAR}}"
        ${Java_JAVA_EXECUTABLE}
        -agentpath:${AGENT_LIB_PATH}
        -cp ${CMAKE_CURRENT_BINARY_DIR}
        TestClass
        DEPENDS test_hook test_java_class jnihook
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running JNIHook test"
        VERBATIM
)

enable_testing()

add_test(
        NAME jnihook_test
        COMMAND ${CMAKE_COMMAND} -E env
        "${LIB_PATH_VAR}=$<TARGET_FILE_DIR:jnihook>${LIB_PATH_SEP}$ENV{${LIB_PATH_VAR}}"
        ${Java_JAVA_EXECUTABLE}
        -agentpath:${AGENT_LIB_PATH}
        -cp ${CMAKE_CURRENT_BINARY_DIR}
        TestClass
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(jnihook_test PROPERTIES
        TIMEOUT 30
        PASS_REGULAR_EXPRESSION "✓ All hooks working correctly!"
)


add_custom_target(test_info
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "JNIHook Test Commands:"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target run_test    # Run test manually"
        COMMAND ${CMAKE_COMMAND} -E echo "  ctest --output-on-failure            # Run via CTest"
        COMMAND ${CMAKE_COMMAND} -E echo ""
        VERBATIM
)