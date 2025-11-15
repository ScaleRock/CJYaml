package com.github.scalerock.cjyaml;

import org.jetbrains.annotations.NotNull;
import org.jetbrains.annotations.Nullable;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

/**
 * High-level wrapper for CJYaml native blob handling.
 * Usage:
 * try (CJYaml parser = new CJYaml()) {
 *     parser.parseFile(path); // default uses DirectByteBuffer
 *     Map<String, Long> header = parser.getHeaderMap();
 *     ...
 * } // native resources freed automatically
 */
public class CJYaml implements AutoCloseable {

    // single native library load guard
    private static volatile boolean nativeLoaded = false;

    // Instance which holds native buffers and JNI wrappers
    private NativeBlob nativeBlob = null;

    // last parsed data (either direct ByteBuffer or byte[])
    private ByteBuffer blobByteBuffer = null;
    private byte[] blobBytes = null;

    // parsed header (lazy)
    private Header header = null;

    /**
     * Ensure native library is loaded once per JVM.
     * Throws UnsatisfiedLinkError on failure.
     */
    private static synchronized void ensureNativeLoaded() {
        if (nativeLoaded) return;

        String osName = System.getProperty("os.name").toLowerCase();
        String libFileName;
        if (osName.contains("win")) {
            libFileName = "cjyaml.dll";
        } else if (osName.contains("mac")) {
            libFileName = "libcjyaml.dylib";
        } else {
            libFileName = "libcjyaml.so";
        }

        String resourcePath = "/" + libFileName;
        try (InputStream in = CJYaml.class.getResourceAsStream(resourcePath)) {
            if (in == null) {
                throw new UnsatisfiedLinkError("Native resource not found: " + resourcePath);
            }
            String suffix = libFileName.substring(libFileName.lastIndexOf('.'));
            File temp = File.createTempFile("libcjyaml", suffix);
            temp.deleteOnExit();
            Files.copy(in, temp.toPath(), StandardCopyOption.REPLACE_EXISTING);
            System.load(temp.getAbsolutePath());
            nativeLoaded = true;
        } catch (IOException e) {
            throw new UnsatisfiedLinkError("Failed to extract/load native library: " + e.getMessage());
        }
    }

    public CJYaml() {
        ensureNativeLoaded();
    }

    /**
     * Parse file and return DirectByteBuffer from native code (default).
     * You must close this CJYaml instance (or use try-with-resources) to free native memory.
     *
     * @param path path to file
     */
    public void parseFile(String path) {
        parseFile(path, true);
    }

    /**
     * Parse file, optionally requesting ByteBuffer or byte[] result.
     * If DirectByteBuffer==true: native DirectByteBuffer is returned and must be freed by native free.
     * If DirectByteBuffer==false: native returns a byte[] (no native free required).
     *
     * @param path            file path
     * @param directByteBuffer whether to ask native side for DirectByteBuffer
     */
    public void parseFile(String path, boolean directByteBuffer) {
        Objects.requireNonNull(path, "path must not be null");

        close(); // release previous resources if any

        nativeBlob = new NativeBlob();

        if (directByteBuffer) {
            blobByteBuffer = nativeBlob.parseToDirectByteBuffer(path);
            blobBytes = null;
        } else {
            blobBytes = nativeBlob.parseToByteArray(path);
            blobByteBuffer = null;
        }

        header = null; // reset parsed header
    }

    /**
     * Return typed Header object parsed from the blob.
     * If no data parsed returns null.
     *
     * @return Header or null
     */
    public Header getHeader() {
        if (header != null) return header;
        if (blobByteBuffer == null && blobBytes == null) return null;

        ByteBuffer buf;
        if (blobByteBuffer != null) {
            buf = blobByteBuffer.duplicate();
        } else {
            buf = ByteBuffer.wrap(blobBytes);
        }
        buf.order(ByteOrder.LITTLE_ENDIAN);

        final int HEADER_SIZE = Header.HEADER_SIZE;
        if (buf.remaining() < HEADER_SIZE) {
            return null;
        }

        Header h = new Header();
        h.magic = Integer.toUnsignedLong(buf.getInt());
        h.version = Short.toUnsignedInt(buf.getShort());
        h.flags = Integer.toUnsignedLong(buf.getInt());

        h.node_table_offset = buf.getLong();
        h.node_count = buf.getLong();

        h.pair_table_offset = buf.getLong();
        h.pair_count = buf.getLong();

        h.index_table_offset = buf.getLong();
        h.index_count = buf.getLong();

        h.hash_index_offset = buf.getLong();
        h.hash_index_size = buf.getLong();

        h.string_table_offset = buf.getLong();
        h.string_table_size = buf.getLong();

        header = h;
        return header;
    }

    /**
     * Convenience: return header as Map<String,Long> (same keys as used earlier).
     *
     * @return map or null
     */
    public Map<String, Long> getHeaderMap() {
        Header h = getHeader();
        if (h == null) return null;
        return h.toMap();
    }

    /**
     * Free native resources (if any). Safe to call multiple times.
     */
    @Override
    public void close() {
        // free native direct buffer inside nativeBlob (if present)
        if (nativeBlob != null) {
            try {
                nativeBlob.close();
            } catch (Exception e) {
                // close should not throw; if it does, wrap to runtime
                throw new RuntimeException("Failed to close native blob", e);
            } finally {
                nativeBlob = null;
            }
        }
        blobByteBuffer = null;
        blobBytes = null;
        header = null;
    }

    // -----------------------------
    // Header typed representation
    // -----------------------------
    public static final class Header {
        // matches C packed HeaderBlob (90 bytes)
        public static final int HEADER_SIZE = 90;

        public long magic;               // uint32 -> stored in long
        public int version;              // uint16 -> stored in int
        public long flags;               // uint32 -> stored in long

        public long node_table_offset;
        public long node_count;

        public long pair_table_offset;
        public long pair_count;

        public long index_table_offset;
        public long index_count;

        public long hash_index_offset;
        public long hash_index_size;

        public long string_table_offset;
        public long string_table_size;

        public @NotNull Map<String, Long> toMap() {
            Map<String, Long> m = new HashMap<>();
            m.put("magic", magic);
            m.put("version", (long) version);
            m.put("flags", flags);

            m.put("node_table_offset", node_table_offset);
            m.put("node_count", node_count);

            m.put("pair_table_offset", pair_table_offset);
            m.put("pair_count", pair_count);

            m.put("index_table_offset", index_table_offset);
            m.put("index_count", index_count);

            m.put("hash_index_offset", hash_index_offset);
            m.put("hash_index_size", hash_index_size);

            m.put("string_table_offset", string_table_offset);
            m.put("string_table_size", string_table_size);

            return m;
        }
    }

    // ---------- Node parsing helpers (add to CJYaml) ----------

    // sizes from C structs
    private static final int NODE_ENTRY_SIZE = 20; // packed: 1 + 1 + 2 + 8 + 8
    private static final int PAIR_ENTRY_SIZE = 8;  // two uint32
    private static final int INDEX_ENTRY_SIZE = 4; // uint32

    // small POJO for NodeEntry
    private static final class NodeEntry {
        int node_type;       // uint8
        int style_flags;     // uint8
        int tag_index;       // uint16
        long a;              // uint64
        long b;              // uint64
    }

    private static final class PairEntry {
        long key_node_index;   // uint32
        long value_node_index; // uint32
    }

    // get a duplicated LE-ordered buffer for absolute reads
    private @NotNull ByteBuffer blobBuf() {
        if (blobByteBuffer != null) {
            return blobByteBuffer.duplicate().order(ByteOrder.LITTLE_ENDIAN);
        } else if (blobBytes != null) {
            return ByteBuffer.wrap(blobBytes).order(ByteOrder.LITTLE_ENDIAN);
        } else {
            throw new IllegalStateException("No blob loaded");
        }
    }

    // read a NodeEntry by index
    @org.jetbrains.annotations.Nullable
    private NodeEntry readNode(int nodeIndex) {
        Header h = getHeader();
        if (h == null) return null;
        long nodeTableOffset = h.node_table_offset;
        long nodeCount = h.node_count;
        if (nodeIndex < 0 || ((long)nodeIndex) >= nodeCount) return null;

        long abs = nodeTableOffset + ((long)nodeIndex) * NODE_ENTRY_SIZE;
        ByteBuffer buf = blobBuf();

        // bounds checks (simple)
        if (abs < 0 || abs + NODE_ENTRY_SIZE > buf.capacity()) return null;

        NodeEntry n = new NodeEntry();
        // ByteBuffer absolute reads require int index; check capacity first
        int pos = (int) abs;
        n.node_type = Byte.toUnsignedInt(buf.get(pos));
        n.style_flags = Byte.toUnsignedInt(buf.get(pos + 1));
        n.tag_index = Short.toUnsignedInt(buf.getShort(pos + 2));
        n.a = buf.getLong(pos + 4);
        n.b = buf.getLong(pos + 12);
        return n;
    }

    // read PairEntry by index
    private @Nullable PairEntry readPair(int pairIndex) {
        Header h = getHeader();
        if (h == null) return null;
        long pairTableOffset = h.pair_table_offset;
        long pairCount = h.pair_count;
        if (pairIndex < 0 || ((long)pairIndex) >= pairCount) return null;

        long abs = pairTableOffset + ((long)pairIndex) * PAIR_ENTRY_SIZE;
        ByteBuffer buf = blobBuf();
        if (abs < 0 || abs + PAIR_ENTRY_SIZE > buf.capacity()) return null;

        PairEntry p = new PairEntry();
        int pos = (int) abs;
        // pair entries are two uint32 little-endian
        p.key_node_index = Integer.toUnsignedLong(buf.getInt(pos));
        p.value_node_index = Integer.toUnsignedLong(buf.getInt(pos + 4));
        return p;
    }

    // read an uint32 index from index table at element idxPos
    private long readIndexTableEntry(long indexTableBase, int idxPos) {
        long abs = indexTableBase + ((long)idxPos) * INDEX_ENTRY_SIZE;
        ByteBuffer buf = blobBuf();
        if (abs < 0 || abs + INDEX_ENTRY_SIZE > buf.capacity()) throw new IndexOutOfBoundsException("index table out of range");
        return Integer.toUnsignedLong(buf.getInt((int) abs));
    }


    // read UTF-8 string from string_table: offset = offset into string table, len = length in bytes
    private @Nullable String readString(long strOffset, long len) {
        Header h = getHeader();
        if (h == null) return null;
        long base = h.string_table_offset;
        long stSize = h.string_table_size;
        if (strOffset < 0 || len < 0 || strOffset + len > stSize) return null;

        ByteBuffer buf = blobBuf();
        long abs = base + strOffset;
        if (abs < 0 || abs + len > buf.capacity()) return null;

        byte[] tmp = new byte[(int) len];
        int pos = (int) abs;
        buf.position(pos);
        buf.get(tmp);
        return new String(tmp, java.nio.charset.StandardCharsets.UTF_8);
    }

    /**
     * Parse the document root and return a Java object representation:
     * - SCALAR -> String
     * - SEQUENCE -> java.util.List<Object>
     * - MAPPING -> java.util.Map<String,Object>  (keys are scalar strings)
     * - DOCUMENT -> returns root node's value
     */
    public Object parseRoot() {
        Header h = getHeader();
        if (h == null) return null;

        // find DOCUMENT node in node table (node_type == 4), take its 'a' as root node index
        for (int i = 0; i < (int) h.node_count; ++i) {
            NodeEntry ne = readNode(i);
            if (ne != null && ne.node_type == 4) { // DOCUMENT
                int rootIndex = (int) ne.a; // a = root_node_index
                return parseNode(rootIndex, 0);
            }
        }
        // fallback: assume node 0 is root
        return parseNode(0, 0);
    }

    // recursive parse node -> Object
    private @Nullable Object parseNode(int nodeIndex, int depth) {
        // avoid infinite recursion
        if (depth > 1024) throw new IllegalStateException("max depth exceeded");

        NodeEntry n = readNode(nodeIndex);
        if (n == null) return null;
        switch (n.node_type) {
            case 0: // SCALAR
                // a = offset into string table, b = length
                return readString(n.a, n.b);
            case 1: { // SEQUENCE
                // a = first_index_into_index_table, b = element_count
                Header h = getHeader();
                int count = (int) n.b;
                long baseIdx = h.index_table_offset;
                java.util.List<Object> list = new java.util.ArrayList<>(Math.max(0, count));
                for (int i = 0; i < count; ++i) {
                    int elemNodeIdx = (int) readIndexTableEntry(baseIdx, (int) (n.a + i));
                    list.add(parseNode(elemNodeIdx, depth + 1));
                }
                return list;
            }
            case 2: { // MAPPING
                // a = first_pair_index_in_pair_table, b = pair_count
                int pairCount = (int) n.b;
                java.util.Map<String, Object> map = new java.util.LinkedHashMap<>();
                for (int i = 0; i < pairCount; ++i) {
                    PairEntry p = readPair((int) (n.a + i));
                    if (p == null) continue;
                    // keys are nodes; expect scalar
                    Object keyObj = parseNode((int) p.key_node_index, depth + 1);
                    String key = (keyObj instanceof String) ? (String) keyObj : String.valueOf(keyObj);
                    Object val = parseNode((int) p.value_node_index, depth + 1);
                    map.put(key, val);
                }
                return map;
            }
            case 3: { // ALIAS
                int target = (int) n.a;
                return parseNode(target, depth + 1);
            }
            case 4: { // DOCUMENT
                int root = (int) n.a;
                return parseNode(root, depth + 1);
            }
            default:
                return null;
        }
    }


    // -----------------------------
    // Native wrapper (static nested)
    // -----------------------------
    private static final class NativeBlob implements AutoCloseable {
        // Only one buffer is tracked per NativeBlob instance (the DirectByteBuffer returned from native).
        private ByteBuffer directBuffer = null;

        private NativeBlob() {
            // constructor left intentionally lightweight; native lib already loaded in outer class
        }

        // JNI declarations - private native methods implemented in your native lib.
        private native ByteBuffer NativeLib_parseToDirectByteBuffer(String path);
        private native byte[] NativeLib_parseToByteArray(String path);
        private native void NativeLib_freeBlob(ByteBuffer buffer);

        ByteBuffer parseToDirectByteBuffer(String path) {
            Objects.requireNonNull(path);
            ByteBuffer b = NativeLib_parseToDirectByteBuffer(path);
            if (b != null) {
                // store the exact object returned by JNI so close() can free it
                this.directBuffer = b;
            }
            return b;
        }

        byte[] parseToByteArray(String path) {
            Objects.requireNonNull(path);
            return NativeLib_parseToByteArray(path);
        }

        @Override
        public void close() {
            if (directBuffer != null) {
                // call native free on the original DirectByteBuffer object
                NativeLib_freeBlob(directBuffer);
                directBuffer = null;
            }
        }
    }
}
