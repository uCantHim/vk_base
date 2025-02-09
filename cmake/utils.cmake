function (torch_default_compile_options TARGET)
    target_compile_features(${TARGET} PUBLIC cxx_std_23)

    if (NOT MSVC)
        target_compile_options(${TARGET} PRIVATE -fPIC)
        target_compile_options(${TARGET} PRIVATE
            $<$<BOOL:${TORCH_BUILD_WITH_DEBUG_INFO}>:-g>
            -O${TORCH_OPTIMIZATION_LEVEL}
        )
        target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic)
    else()
        target_compile_options(${TARGET} PRIVATE
            /W4
            /O${TORCH_OPTIMIZATION_LEVEL}
        )
        if (POLICY CMP0091)
            cmake_policy(SET CMP0091 NEW)
        endif ()
    endif()

    # Generate code coverage when using GCC
    if (${TORCH_GENERATE_CODE_COVERAGE})
        if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${TARGET} PRIVATE $<$<CONFIG:Debug>:-fprofile-arcs -ftest-coverage --coverage>)
            target_link_options(${TARGET} PRIVATE $<$<CONFIG:Debug>:--coverage>)
            target_link_libraries(${TARGET} PRIVATE gcov)
        else()
            message(WARNING "Code coverage generation was enabled by setting TORCH_GENERATE_CODE_COVERAGE"
                            " to ON, but this feature is only supported for GCC. Disabling code coverage"
                            " generation.")
        endif()
    endif ()
endfunction()

function (link_gtest TARGET)
    find_package(GTest)
    if (${GTEST_FOUND})
        target_link_libraries(${TARGET} PRIVATE GTest::GTest GTest::Main)
    else ()
        if (NOT TARGET GTest::gtest_main OR NOT TARGET GTest::gmock_main)
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest
                GIT_TAG main
            )
            # For Windows: Prevent overriding the parent project's compiler/linker settings
            set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

            set(BUILD_GMOCK TRUE)
            set(INSTALL_GTEST FALSE)
            FetchContent_MakeAvailable(googletest)
        endif ()

        target_link_libraries(${TARGET} PRIVATE GTest::gtest_main GTest::gmock_main)
    endif ()
endfunction()
