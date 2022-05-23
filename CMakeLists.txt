cmake_minimum_required(VERSION 3.15...3.22)

project(fastvarints VERSION "0.0.1")

if(SKBUILD)
  # Scikit-Build does not add your site-packages to the search path
  # automatically, so we need to add it _or_ the pybind11 specific directory
  # here.
  execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" -c
            "import pybind11; print(pybind11.get_cmake_dir())"
    OUTPUT_VARIABLE _tmp_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ECHO STDOUT)
  list(APPEND CMAKE_PREFIX_PATH "${_tmp_dir}")
endif()

# Now we can find pybind11
#find_package(pybind11 CONFIG REQUIRED)
add_subdirectory("extern/pybind11")

pybind11_add_module(_core MODULE src/main.cpp)
target_compile_definitions(_core PRIVATE VERSION_INFO=${PROJECT_VERSION})

install(TARGETS _core DESTINATION .)

#target_link_libraries(_core PUBLIC pybind11)
install(TARGETS _core DESTINATION .)

# this is optional to enable editable mode
add_custom_command(
        TARGET _core
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:_core>
        ${CMAKE_CURRENT_LIST_DIR}/src/fastvarints/$<TARGET_FILE_NAME:_core>
)