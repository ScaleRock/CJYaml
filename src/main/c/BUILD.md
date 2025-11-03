This document explains how to compile the source file `src/CJYaml.c` into shared libraries for Linux and Windows using a Makefile.

---

## 1. Requirements

Make sure you have installed:

* **Linux** (e.g., Rocky Linux)
* **GCC** for Linux: `gcc`
* **MinGW-w64** for Windows: `x86_64-w64-mingw32-gcc`
* **make**

### Installing MinGW on Rocky Linux:

```bash
sudo dnf install epel-release
sudo dnf install mingw64-gcc mingw64-gcc-c++
```

---

## 2. Directory Structure

Your project should look approximately like this:

```
project/
│
├─ src/
│   └─ CJYaml.c
│
├─ out/           # this folder will be created automatically by the Makefile
├─ Makefile
└─ BUILD.md
```

---

## 3. Makefile

The Makefile uses the following variables:

```makefile
CC_LINUX = gcc
CC_WINDOWS = x86_64-w64-mingw32-gcc
CFLAGS = -O2 -Wall -Wextra -Wpedantic -Wshadow -fPIC
```

It builds two libraries:

* `out/cjyaml.so` – for Linux
* `out/cjyaml.dll` – for Windows

---

## 4. Compilation

Run the following command in the project directory:

```bash
make
```

The Makefile will automatically:

1. Create the `out/` directory if it does not exist
2. Compile `src/CJYaml.c` into:

   * `out/cjyaml.so` – Linux `.so`
   * `out/cjyaml.dll` – Windows `.dll`

---

## 5. Checking the Results

After compilation, check the `out/` directory:

```bash
ls out/
```

You should see:

```
cjyaml.so
cjyaml.dll
```

---

## 6. Cleaning the Build

To remove the compiled files:

```bash
make clean
```

This will delete the `out/` directory along with its contents.

---

## 7. Tips

* To enable debugging, add the `-g` flag to `CFLAGS`.
* To treat warnings as errors, add `-Werror`.
* The `.dll` created by MinGW can be tested on Windows or using Wine.

