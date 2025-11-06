# Minimal Mingw toolchain for cross-build to Windows (x86_64)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Adjust paths to your cross-compiler installation if needed
set(CMAKE_C_COMPILER   /usr/bin/x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++)

# Optional resource compiler
set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres)

# Optionally set ar/ranlib if needed:
# set(CMAKE_AR /usr/bin/x86_64-w64-mingw32-ar)
# set(CMAKE_RANLIB /usr/bin/x86_64-w64-mingw32-ranlib)

# Find settings - allow find_* to look at target root path for libs/includes:
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
