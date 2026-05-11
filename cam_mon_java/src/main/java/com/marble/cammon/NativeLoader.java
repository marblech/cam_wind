package com.marble.cammon;

import java.io.InputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Utility to load native libraries packaged inside the JAR by extracting
 * them to a temporary file and calling System.load().
 */
public final class NativeLoader {
    private NativeLoader() {}

    public static void loadFromJar(String resourcePath, String tmpPrefix) throws IOException {
        InputStream is = NativeLoader.class.getResourceAsStream(resourcePath);
        if (is == null) throw new IOException("Resource not found: " + resourcePath);
        Path tmp = Files.createTempFile(tmpPrefix, resourcePath.endsWith(".dll") ? ".dll" : null);
        tmp.toFile().deleteOnExit();
        Files.copy(is, tmp, StandardCopyOption.REPLACE_EXISTING);
        is.close();
        System.load(tmp.toAbsolutePath().toString());
    }
}
