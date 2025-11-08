# CJYaml

This is a parser for files in the yaml (1.2.2) format. CJ stands for c and java. All logic and parsing are written in pure C for maximum performance. They are compiled into DLL and SO files and then loaded using a simple Java "bridge" that allows efficient work with YAML files.



Jasne! Oto krótka wersja po angielsku do README:

---

## Building the Project

### Linux (native)

```bash
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DENABLE_JNI=ON ..

cmake --build . --config Release
```

Output: `out/linux/`

* Shared library (if enabled): `libcjyaml.so`

Copy the library to src/main/java/resources

### Windows (native)

```cmd
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DENABLE_JNI=ON ..

cmake --build . --config Release
```

Output: `out/windows/`

* Shared library: `cjyaml.dll`

> Optional: Set `JAVA_WINDOWS_HOME` for JNI support on Windows.

Copy the library to src/main/java/resources
### Clean

```bash
cmake --build . --target clean-all
```

Removes the `out/` directory and build artifacts.



### Third-party components

This project includes the xxHash library:

- Name: xxHash
- Author: Yann Collet
- License: BSD 2-Clause
- URL: https://github.com/Cyan4973/xxHash
- Copyright © 2012-2021 Yann Collet
