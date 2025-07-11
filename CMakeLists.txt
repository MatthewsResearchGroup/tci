cmake_minimum_required(VERSION 3.20)

set(TCI_VERSION_MAJOR 1)
set(TCI_VERSION_MINOR 0)
set(TCI_VERSION "${TCI_VERSION_MAJOR}.${TCI_VERSION_MINOR}")
set(prefix ${CMAKE_INSTALL_PREFIX})

project(TCI
    VERSION "${TCI_VERSION}"
    LANGUAGES C CXX
)

include(CheckIncludefile)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

#
# Set up options
#

set(ENABLE_MUTEX auto CACHE STRING "use a mutex of the specified type, one of auto, pthread_mutex, \
pthread_spinlock, osx_spinlock, os_unfair_lock, omp_lock, atomic_spinlock [default=auto]")

set(ENABLE_BARRIER auto CACHE STRING "use a barrier of the specified type, one of auto, pthread_barrier, \
spin_barrier [default=auto]")

set(ENABLE_THREAD_MODEL auto CACHE STRING "enable threading with the specified model, one of none, auto, task, \
openmp, pthreads, tbb, omptask, winthreads, ppl, or dispatch [default=auto]")

set(ENABLE_THREADING auto CACHE STRING "synonym for ENABLE_THREAD_MODEL")

if(ENABLE_THREAD_MODEL STREQUAL auto AND NOT ENABLE_THREADING STREQUAL auto
    set(ENABLE_THREAD_MODEL ${ENABLE_THREADING})
endif()

# Default to disabled for all mutex/barrier/thread options
set(USE_PTHREAD_MUTEX 0)
set(USE_PTHREAD_SPINLOCK 0)
set(USE_OSX_SPINLOCK 0)
set(USE_OMP_LOCK 0)
set(USE_ATOMIC_SPINLOCK 0)

set(USE_PTHREAD_BARRIER 0)
set(USE_SPIN_BARRIER 0)

set(USE_PTHREADS_THREADS 0)
set(USE_OPENMP_THREADS 0)
set(USE_TBB_THREADS 0)
set(USE_PPL_THREADS 0)
set(USE_OMPTASK_THREADS 0)
set(USE_DISPATCH_THREADS 0)
set(USE_WINDOWS_THREADS 0)

#
# Check for various libraries
#

# Detect pthread support
find_package(Threads)

# Check for OpenMP support
find_package(OpenMP)

# Check for Apple GCD (libdispatch on BSD)
check_include_file(dispatch/dispatch.h HAVE_DISPATCH_H)
check_library_exists(dispatch dispatch_group_async_f "" HAVE_DISPATCH_LIB)

# Check for TBB
find_package(TBB QUIET)

# Check for Windows/PPL
check_include_file(windows.h HAVE_WINDOWS_H)
check_include_file(ppl.h HAVE_PPL_H)

# Check for OSX/macOS spin locks
check_include_file(os/lock.h HAVE_OS_LOCK_H)
check_symbol_exists(os_unfair_lock_lock os/lock.h HAVE_OS_UNFAIR_LOCK_LOCK)
check_include_file(libkern/OSAtomic.h HAVE_LIBKERN_OSATOMIC_H)
check_symbol_exists(OSSpinLockLock libkern/OSAtomic.h HAVE_OSSPINLOCKLOCK)
check_library_exists(atomic __atomic_compare_exchange_16 "" HAVE_ATOMIC_LIB)

# Verify availability of GCC style __atomic builtins
check_c_source_compiles("
    int main()
    {
        // Test all atomic builtin functions
        __atomic_load_n((int*)0, __ATOMIC_SEQ_CST);
        __atomic_load((int*)0, (int*)0, __ATOMIC_SEQ_CST);
        __atomic_store_n((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_store((int*)0, (int*)0, __ATOMIC_SEQ_CST);
        __atomic_exchange_n((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_exchange((int*)0, (int*)0, (int*)0, __ATOMIC_SEQ_CST);
        __atomic_compare_exchange_n((int*)0, (int*)0, 0, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        __atomic_compare_exchange((int*)0, (int*)0, (int*)0, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        __atomic_add_fetch((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_sub_fetch((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_and_fetch((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_xor_fetch((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_or_fetch((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_nand_fetch((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_fetch_add((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_fetch_sub((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_fetch_and((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_fetch_xor((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_fetch_or((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_fetch_nand((int*)0, 1, __ATOMIC_SEQ_CST);
        __atomic_test_and_set((char*)0, __ATOMIC_SEQ_CST);
        __atomic_clear((_Bool*)0, __ATOMIC_SEQ_CST);
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        __atomic_signal_fence(__ATOMIC_SEQ_CST);
        __atomic_always_lock_free(4, 0);
        __atomic_is_lock_free(4, 0);
        return 0;
    }"
    HAVE_GCC_ATOMIC_BUILTINS
)
if(NOT HAVE_GCC_ATOMIC_BUILTINS)
    message(FATAL_ERROR "__atomic builtins required")
endif()


if(CMAKE_USE_PTHREADS_INIT)
    check_symbol_exists(pthread_mutex_init pthread.h HAVE_PTHREAD_MUTEX_INIT)
    check_symbol_exists(pthread_spin_init pthread.h HAVE_PTHREAD_SPIN_INIT)
    check_symbol_exists(pthread_barrier_init pthread.h HAVE_PTHREAD_BARRIER_INIT)
endif()

# Detect pthreads
if(CMAKE_USE_PTHREADS_INIT)
    set(HAVE_PTHREADS_THREADS 1)
endif()

# Detect OpenMP
if(OpenMP_C_FOUND)
    set(HAVE_OPENMP_THREADS 1)
    # This really should be check specifically
    set(HAVE_OMPTASK_THREADS 1)
    set(HAVE_OMP_LOCK 1)
endif()

# Detect libdispatch (Apple GCD)
if(HAVE_DISPATCH_H AND (HAVE_DISPATCH_LIB OR APPLE))
    set(HAVE_DISPATCH_THREADS 1)
endif()

# Detect TBB
if(TBB_FOUND)
    set(HAVE_TBB_THREADS 1)
endif()

# Detect Windows
if(WIN32 AND HAVE_WINDOWS_H)
    set(HAVE_WINDOWS_THREADS 1)
endif()

# Detect PPL
if(WIN32 AND HAVE_WINDOWS_H AND HAVE_PPL_H)
    set(HAVE_PPL_THREADS 1)
endif()

# Detect pthread_mutex
if(CMAKE_USE_PTHREADS_INIT AND HAVE_PTHREAD_MUTEX_INIT)
    set(HAVE_PTHREAD_MUTEX 1)
endif()

# Detect pthread_spin
if(CMAKE_USE_PTHREADS_INIT AND HAVE_PTHREAD_SPIN_INIT)
    set(HAVE_PTHREAD_SPINLOCK 1)
endif()

# Detect pthread_barrier
if(CMAKE_USE_PTHREADS_INIT AND HAVE_PTHREAD_BARRIER_INIT)
    set(HAVE_PTHREAD_BARRIER 1)
endif()

# Detect os_unfair_lock
if(HAVE_OS_LOCK_H AND HAVE_OS_UNFAIR_LOCK_LOCK)
    set(HAVE_OS_UNFAIR_LOCK 1)
endif()

# Detect OSSpinLock
if(HAVE_LIBKERN_OSATOMIC_H AND HAVE_OSSPINLOCKLOCK)
    set(HAVE_OSX_SPINLOCK 1)
endif()

#
# Determine mutex type
#

if(ENABLE_MUTEX STREQUAL auto)
    if(HAVE_OS_UNFAIR_LOCK)
        set(ENABLE_MUTEX os_unfair_lock)
    elseif(HAVE_OSX_SPINLOCK)
        set(ENABLE_MUTEX osx_spinlock)
    elseif(HAVE_PTHREAD_SPINLOCK)
        set(ENABLE_MUTEX pthread_spinlock)
    else()
        set(ENABLE_MUTEX atomic_spinlock)
    endif()
endif()
message(STATUS "mutex type selected: ${ENABLE_MUTEX}")

if(ENABLE_MUTEX STREQUAL pthread_mutex)
    if(NOT HAVE_PTHREAD_MUTEX)
        message(FATAL_ERROR "pthread mutex requested but not available")
    endif()
    set(USE_PTHREAD_MUTEX 1)
elseif(ENABLE_MUTEX STREQUAL pthread_spinlock)
    if(NOT HAVE_PTHREAD_SPINLOCK)
        message(FATAL_ERROR "pthread spinlock requested but not available")
    endif()
    set(USE_PTHREAD_SPINLOCK 1)
elseif(ENABLE_MUTEX STREQUAL os_unfair_lock)
    if(NOT HAVE_OS_UNFAIR_LOCK)
        message(FATAL_ERROR "macOS os_unfair_lock requested but not available")
    endif()
    set(USE_OS_UNFAIR_LOCK 1)
elseif(ENABLE_MUTEX STREQUAL osx_spinlock)
    if(NOT HAVE_OSX_SPINLOCK)
        message(FATAL_ERROR "OSX OSSpinLock requested but not available")
    endif()
    set(USE_OSX_SPINLOCK 1)
elseif (ENABLE_MUTEX STREQUAL omp_lock)
    if(NOT HAVE_OMP_LOCK)
        message(FATAL_ERROR "omp_lock requested but not available")
    endif()
    set(USE_OMP_LOCK 1)
elseif (ENABLE_MUTEX STREQUAL atomic_spinlock)
    set(USE_ATOMIC_SPINLOCK 1)
else()
    message(FATAL_ERROR "invalid mutex type specified")
endif()

#
# Determine barrier type
#

if(ENABLE_BARRIER STREQUAL auto)
    set(ENABLE_BARRIER spin_barrier)
endif()

if(ENABLE_BARRIER STREQUAL pthread_barrier)
    if(NOT HAVE_PTHREAD_BARRIER)
        message(FATAL_ERROR "pthread barrier requested but not available")
    endif()
    set(USE_PTHREAD_BARRIER 1)
elseif(ENABLE_BARRIER STREQUAL spin_barrier)
    set(USE_SPIN_BARRIER 1)
else()
    message(FATAL_ERROR "invalid barrier type specified")
endif()
message(STATUS "barrier type selected: ${ENABLE_BARRIER}")

#
# Determine thread model
#

if(ENABLE_THREAD_MODEL STREQUAL auto)
    if(HAVE_OPENMP_THREADS)
        set(ENABLE_THREAD_MODEL openmp)
    elseif(HAVE_PTHREADS_THREADS)
        set(ENABLE_THREAD_MODEL pthread)
    elseif(HAVE_WINDOWS_THREADS)
        set(ENABLE_THREAD_MODEL windows)
    elseif(HAVE_TBB_THREADS)
        set(ENABLE_THREAD_MODEL tbb)
    elseif(HAVE_PPL_THREADS)
        set(ENABLE_THREAD_MODEL ppl)
    elseif(HAVE_DISPATCH_THREADS)
        set(ENABLE_THREAD_MODEL dispatch)
    else()
        set(ENABLE_THREAD_MODEL none)
    endif()
elseif(ENABLE_THREAD_MODEL STREQUAL task)
    if(HAVE_TBB_THREADS)
        set(ENABLE_THREAD_MODEL tbb)
    elseif(HAVE_PPL_THREADS)
        set(ENABLE_THREAD_MODEL ppl)
    elseif(HAVE_DISPATCH_THREADS)
        set(ENABLE_THREAD_MODEL dispatch)
    elseif(HAVE_OMPTASK_THREADS)
        set(ENABLE_THREAD_MODEL omptask)
    else()
        set(ENABLE_THREAD_MODEL none)
    endif()
endif()
message(STATUS "thread model selected: ${ENABLE_THREAD_MODEL}")

if(ENABLE_THREAD_MODEL STREQUAL openmp)
    if(NOT HAVE_OPENMP_THREADS)
        message(FATAL_ERROR "openmp requested but not available")
    endif()
    set(USE_OPENMP_THREADS 1)
elseif(ENABLE_THREAD_MODEL STREQUAL omptask)
    if(NOT HAVE_OMPTASK_THREADS)
        message(FATAL_ERROR "omptask requested but not available")
    endif()
    set(USE_OMPTASK_THREADS 1)
elseif(ENABLE_THREAD_MODEL STREQUAL pthread OR
       ENABLE_THREAD_MODEL STREQUAL pthreads)
    if(NOT HAVE_PTHREADS_THREADS)
        message(FATAL_ERROR "pthreads requested but not available")
    endif()
    set(USE_PTHREADS_THREADS 1)
elseif(ENABLE_THREAD_MODEL STREQUAL tbb)
    if(NOT HAVE_TBB_THREADS)
        message(FATAL_ERROR "tbb requested but not available")
    endif()
    set(USE_TBB_THREADS 1)
elseif(ENABLE_THREAD_MODEL STREQUAL dispatch OR
       ENABLE_THREAD_MODEL STREQUAL libdispatch OR
       ENABLE_THREAD_MODEL STREQUAL gcd)
    if(NOT HAVE_DISPATCH_THREADS)
        message(FATAL_ERROR "dispatch requested but not available")
    endif()
    set(USE_DISPATCH_THREADS 1)
elseif(ENABLE_THREAD_MODEL STREQUAL windows OR
       ENABLE_THREAD_MODEL STREQUAL winthreads)
    if(NOT HAVE_WINDOWS_THREADS)
        message(FATAL_ERROR "winthreads requested but not available")
    endif()
    set(USE_WINDOWS_THREADS 1)
elseif(ENABLE_THREAD_MODEL STREQUAL ppl)
    if(NOT HAVE_PPL_THREADS)
        message(FATAL_ERROR "ppl requested but not available")
    endif()
    set(USE_PPL_THREADS 1)
elseif(NOT ENABLE_THREAD_MODEL STREQUAL none)
    message(FATAL_ERROR "Invalid threading model specified.")
endif()

#
# Determine required libraries and/or flags
#

if(HAVE_ATOMIC_LIB)
    list(APPEND EXTRA_LIBS atomic)
endif()

if(USE_PTHREADS_THREADS OR USE_PTHREAD_MUTEX OR USE_PTHREAD_BARRIER OR USE_PTHREAD_SPINLOCK)
    list(APPEND EXTRA_LIBS Threads::Threads)
endif()

if(USE_OPENMP_THREADS OR USE_OMP_LOCK)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    list(APPEND EXTRA_LIBS OpenMP::OpenMP_C)
endif()

if(USE_DISPATCH_THREADS AND HAVE_DISPATCH_LIB)
    list(APPEND EXTRA_LIBS dispatch)
endif()

if(USE_TBB_THREADS)
    list(APPEND EXTRA_LIBS TBB::tbb)
endif()

if(USE_WINDOWS_THREADS)
    # no additional libraries required
endif()

if(USE_PPL_THREADS)
    # no additional libraries required
endif()

#
# Configure TCI
#

add_library(tci SHARED
    tci/barrier.c
    tci/communicator.c
    tci/task_set.c
    tci/context.c
    tci/mutex.c
    tci/parallel.c
    tci/slot.c
    tci/work_item.c
)

set_target_properties(tci PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(tci PUBLIC ${EXTRA_LIBS})

set_target_properties(tci PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

configure_file(tci/tci_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/tci/tci_config.h @ONLY)
configure_file(tci.pc.in ${CMAKE_CURRENT_BINARY_DIR}/tci.pc @ONLY)

# Have to compile communicator and task_set with C++ if using TBB or PPL
if(USE_PPL_THREADS OR USE_TBB_THREADS)
    set(TCI_CXX ON)
endif()

if(TCI_CXX)
    set_source_files_properties(tci/communicator.c PROPERTIES LANGUAGE CXX)
    set_source_files_properties(tci/task_set.c PROPERTIES LANGUAGE CXX)
endif()

target_include_directories(tci PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tci>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/tci>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_sources(tci
    INTERFACE FILE_SET public_headers TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
    tci/barrier.h
    tci/communicator.h
    tci/context.h
    tci/mutex.h
    tci/parallel.h
    tci/pipeline.h
    tci/slot.h
    tci/task_set.h
    tci/tci_global.h
    tci/work_item.h
    tci/yield.h
    tci.h
)

target_sources(tci
    INTERFACE FILE_SET config_headers TYPE HEADERS
    BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tci/tci_config.h
)

install(TARGETS tci EXPORT tci-targets
    FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET config_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT tci-targets
    FILE TCITargets.cmake
    NAMESPACE TCI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TCI)

write_basic_package_version_file(
    "TCIConfigVersion.cmake"
    VERSION ${CMAKE_PACKAGE_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(FILES "TCIConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/TCIConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TCI)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tci.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
