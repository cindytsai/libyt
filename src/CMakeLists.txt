# find dependencies
find_package(
  Python 3.7
  COMPONENTS Development NumPy
  REQUIRED
)
if (USE_PYBIND11)
  find_package(pybind11 REQUIRED)
endif ()

if (NOT SERIAL_MODE)
  find_package(MPI REQUIRED)
endif ()

if (JUPYTER_KERNEL)
  # since vendor/CMakeLists.txt has already checked the version requirement or populated
  # xeus-zmq, we don't need to check it again here.
  find_package(xeus-zmq QUIET)
endif ()

# old tangled libyt stuff combined as a whole (will definitely split to smaller modules in
# future version)
add_library(
  yt SHARED
  comm_mpi.cpp
  comm_mpi_rma.cpp
  data_hub_amr.cpp
  data_structure_amr.cpp
  dtype_utilities.cpp
  function_info.cpp
  init_libyt_module.cpp
  init_python.cpp
  libyt_kernel.cpp
  libyt_process_control.cpp
  libyt_python_module.cpp
  libyt_python_shell.cpp
  libyt_worker.cpp
  logging.cpp
  magic_command.cpp
  numpy_controller.cpp
  py_add_dict.cpp
  timer.cpp
  timer_control.cpp
  utilities.cpp
  yt_commit.cpp
  yt_finalize.cpp
  yt_free.cpp
  yt_get_FieldsPtr.cpp
  yt_get_GridsPtr.cpp
  yt_get_ParticlesPtr.cpp
  yt_getGridInfo.cpp
  yt_initialize.cpp
  yt_run.cpp
  yt_run_InteractiveMode.cpp
  yt_run_JupyterKernel.cpp
  yt_run_ReloadScript.cpp
  yt_set_Parameters.cpp
  yt_set_UserParameter.cpp
)

# set code coverage and add yt as target
add_library(coverage_config INTERFACE)
if (CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Add required flags (GCC & LLVM/Clang)
  target_compile_options(
    coverage_config
    INTERFACE
      -O0 # no optimization
      -g # generate debug info
      --coverage # sets all required flags
  )
  target_link_options(coverage_config INTERFACE --coverage)
endif (CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
target_link_libraries(yt PUBLIC coverage_config)

# set and print options and apply compile definition -D
function (set_option option_name)
  if (${option_name})
    message(STATUS "${option_name} is ON")
    target_compile_definitions(yt PUBLIC ${option_name})
  else ()
    message(STATUS "${option_name} is OFF")
  endif ()
endfunction ()

set_option(SERIAL_MODE)
set_option(INTERACTIVE_MODE)
set_option(JUPYTER_KERNEL)
set_option(SUPPORT_TIMER)
set_option(USE_PYBIND11)

# include dir
target_include_directories(
  yt
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    $<$<NOT:$<BOOL:${SERIAL_MODE}>>:${MPI_CXX_INCLUDE_DIRS}> ${Python_INCLUDE_DIRS}
    $<$<BOOL:${INTERACTIVE_MODE}>:${Readline_INCLUDE_DIR}>
)

# link lib
target_link_libraries(
  yt
  PRIVATE
    $<$<NOT:$<BOOL:${SERIAL_MODE}>>:MPI::MPI_CXX>
    ${Python_LIBRARIES}
    $<$<BOOL:${USE_PYBIND11}>:pybind11::embed>
    Python::NumPy
    $<$<BOOL:${INTERACTIVE_MODE}>:${Readline_LIBRARY}>
    $<$<BOOL:${INTERACTIVE_MODE}>:${Readline_LIBFLAGS}>
    $<$<BOOL:${JUPYTER_KERNEL}>:xeus-zmq>
)

# set properties
set(LIBYT_PUBLIC_HEADER
    "${PROJECT_SOURCE_DIR}/include/libyt.h;"
    "${PROJECT_SOURCE_DIR}/include/yt_type.h;"
    "${PROJECT_SOURCE_DIR}/include/yt_type_param_libyt.h;"
    "${PROJECT_SOURCE_DIR}/include/yt_type_param_yt.h;"
    "${PROJECT_SOURCE_DIR}/include/yt_type_grid.h;"
    "${PROJECT_SOURCE_DIR}/include/yt_type_field.h;"
    "${PROJECT_SOURCE_DIR}/include/yt_type_particle.h;"
    "${PROJECT_SOURCE_DIR}/include/yt_type_array.h;"
    "${PROJECT_SOURCE_DIR}/include/yt_macro.h;"
)
set_target_properties(
  yt
  PROPERTIES
    VERSION ${PROJECT_VERSION} # version
    SOVERSION ${PROJECT_VERSION_MAJOR} # soversion (.so.x.y.z)
    PUBLIC_HEADER "${LIBYT_PUBLIC_HEADER}" # APIs
    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS} # build PIC
    INSTALL_RPATH_USE_LINK_PATH TRUE # set rpath
)

# set rpath
if (JUPYTER_KERNEL)
  set(LIBYT_VENDOR_RPATH "")

  if (LIBYT_GET_ZEROMQ_BY_VENDOR)
    list(APPEND LIBYT_VENDOR_RPATH "${PROJECT_SOURCE_DIR}/vendor/libzmq/libzmq-build/lib")
  endif ()

  if (LIBYT_GET_XEUS_BY_VENDOR)
    list(APPEND LIBYT_VENDOR_RPATH "${PROJECT_SOURCE_DIR}/vendor/xeus/xeus-build")
  endif ()

  if (LIBYT_GET_XEUS_ZMQ_BY_VENDOR)
    list(APPEND LIBYT_VENDOR_RPATH "${PROJECT_SOURCE_DIR}/vendor/xeus-zmq/xeus-zmq-build")
  endif ()

  set_target_properties(yt PROPERTIES INSTALL_RPATH "${LIBYT_VENDOR_RPATH}")
endif ()

# INSTALLATION RULE #
include(GNUInstallDirs)
install(
  TARGETS yt
  LIBRARY DESTINATION lib
  PUBLIC_HEADER DESTINATION include
)
