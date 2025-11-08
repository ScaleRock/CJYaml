package com.github.scalerock.cjyaml;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

public class CJYaml {

    static {
        try {
            loadLibraryFromResources();
        } catch (IOException e) {
            throw new UnsatisfiedLinkError("Failed to load native library: " + e.getMessage());
        }
    }

    private static void loadLibraryFromResources() throws IOException {
        String osName = System.getProperty("os.name").toLowerCase();
        String libFileName;

        if (osName.contains("win")) {
            libFileName = "cjyaml" + ".dll";
        } else if (osName.contains("mac")) {
            libFileName = "lib" + "cjyaml" + ".dylib";
        } else {
            libFileName = "lib" + "cjyaml" + ".so";
        }

        String resourcePath = "/" + libFileName;
        InputStream in = CJYaml.class.getResourceAsStream(resourcePath);
        if (in == null) {
            throw new FileNotFoundException("Resource not found: " + resourcePath);
        }

        // Utwórz plik tymczasowy
        File temp = File.createTempFile("libcjyaml", libFileName.substring(libFileName.lastIndexOf('.')));
        temp.deleteOnExit();

        Files.copy(in, temp.toPath(), StandardCopyOption.REPLACE_EXISTING);

        System.load(temp.getAbsolutePath());
    }

    // -------------------------
    // JNI methods
    // -------------------------
    public static native ByteBuffer NativeLib_parseToDirectByteBuffer(String path);
    public static native byte[]  NativeLib_parseToByteArray(byte[] data);
    public static native void NativeLib_freeBlob(ByteBuffer buffer);

}
