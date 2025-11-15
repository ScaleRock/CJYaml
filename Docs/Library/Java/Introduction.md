# Java CJYaml Library Documentation

## Introduction

CJYaml is a high‑performance Java wrapper for a native YAML‑like binary format parser. It provides:

* automatic native library loading
* fast zero‑copy parsing using DirectByteBuffer
* optional parsing to byte[]
* a high‑level tree representation of the parsed document
* automatic cleanup with `AutoCloseable`

The library is designed for applications that require fast and memory‑efficient processing of structured YAML‑derived binary data.

## Getting Started

### Basic Example

```java
import java.util.Map;
import com.github.scalerock.cjyaml.CJYaml;

public class ParsingFirstFile {
    public static void main(String[] args) {
        final String path = "example.yaml";

        try (CJYaml yaml = new CJYaml()) {
            yaml.parseFile(path);
            Map<?, ?> data = (Map<?, ?>) yaml.parseRoot();
            Object value = data.get("key");
            if (value instanceof String) {
                System.out.println(value);
            }
        }
    }
}
```

## Native Library Loading

CJYaml automatically loads its native library (`libcjyaml.so`, `cjyaml.dll`, or `libcjyaml.dylib`) from packaged resources.

* The library is extracted to a temporary file.
* Loaded once per JVM using `System.load()`.
* Extraction errors or missing binaries throw `UnsatisfiedLinkError`.

## Parsing Files

### `parseFile(String path)`

Parses a file and requests a native `DirectByteBuffer`.
This mode offers the highest performance and minimal copying.

### `parseFile(String path, boolean directByteBuffer)`

* `true` — returns a DirectByteBuffer allocated natively; must be freed via `close()`.
* `false` — returns a `byte[]`; does not require native freeing.

Calling `parseFile` again automatically releases previous native resources.

## Memory Management

CJYaml implements `AutoCloseable`:

```java
try (CJYaml parser = new CJYaml()) {
    ...
} // native memory freed automatically
```

You can also close manually if you need early cleanup:

```java
CJYaml parser = new CJYaml();
parser.parseFile("file.yaml");

if (errorDetected) {
    parser.close(); // safe to call multiple times
}
```

## Header Parsing

The binary blob begins with a fixed‑size (90‑byte) header. The `Header` class decodes:

* magic
* version
* flags
* offsets and sizes of node, pair, index, hash, and string tables

Accessing the header:

```java
Header h = yaml.getHeader();
Map<String, Long> map = yaml.getHeaderMap();
```

Both return `null` if no blob is loaded.

## Document Model

CJYaml parses the binary document into standard Java structures:

* **SCALAR (0)** → `String`
* **SEQUENCE (1)** → `List<Object>`
* **MAPPING (2)** → `Map<String, Object>`
* **ALIAS (3)** → resolved recursively
* **DOCUMENT (4)** → returns its root node

The entire structure can be obtained via:

```java
Object root = yaml.parseRoot();
```

## Internal Structures

### Node Table

Each node entry (20 bytes) contains:

* type
* style flags
* tag index
* two 64‑bit values (`a`, `b`)

### Pair Table

Mapping entries (8 bytes) include:

* key node index
* value node index

### Index Table

Sequences store indexes into the node table.

### String Table

Contains UTF‑8 encoded strings referenced by scalar nodes.

## Native Layer

Native parsing is handled by `NativeBlob`, which:

* holds the native DirectByteBuffer reference
* provides JNI methods:

    * `NativeLib_parseToDirectByteBuffer`
    * `NativeLib_parseToByteArray`
    * `NativeLib_freeBlob`

Native memory belonging to the DirectByteBuffer is freed during `close()`.

## Error Handling

* Calling `close()` multiple times is safe.
* Failure in native `close()` is wrapped into `RuntimeException`.
* Attempting operations without a loaded blob throws `IllegalStateException`.
* Deep recursion (>1024) throws `IllegalStateException`.

## Example: Using byte[] Mode

```java
try (CJYaml yaml = new CJYaml()) {
    yaml.parseFile("example.yaml", false); // parse into byte[]
    Object data = yaml.parseRoot();
    System.out.println(data);
}
```

Useful when working in environments where direct native memory allocation is restricted.

## Troubleshooting

### "Native resource not found"

Ensure the required `.so`, `.dll`, or `.dylib` file is included in your JAR under root `/`.

### "Failed to extract/load native library"

Occurs when:

* filesystem is read‑only
* application lacks write permission to temp directory

### Null results

* `getHeader()` returns `null` if blob smaller than 90 bytes
* `parseRoot()` returns `null` if root node not found

### Releasing resources early

You may call:

```java
yaml.close();
```

to free DirectByteBuffer immediately when needed.

## Summary

CJYaml provides a fast, lightweight, native‑accelerated parser for structured YAML‑derived binary files. It is safe, memory‑efficient, and simple to integrate into Java applications, particularly when working with large or performance‑critical data structures.
