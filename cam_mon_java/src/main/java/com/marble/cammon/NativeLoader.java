package com.marble.cammon;

import java.io.InputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/**
 * Utility to load native libraries packaged inside the JAR by extracting
 * them to a temporary directory.
 *
 * Supports two loading modes:
 * <ul>
 *   <li>{@link #loadFromJar(String, String)} - extracts a single library and
 *       loads it via {@code System.load()}, suitable for JNI-style loading.</li>
 *   <li>{@link #extractNativeLibraries()} - extracts all native libraries under
 *       {@code /native/} to a temp directory and configures
 *       {@code jna.library.path} so that JNA can find them automatically.</li>
 * </ul>
 */
public final class NativeLoader {

    /** Temp directory that holds extracted native libraries; created once. */
    private static Path nativeTempDir;

    private NativeLoader() {}

    /**
     * Extract all native libraries from the JAR's {@code /native/} resource
     * path into a temporary directory, set {@code jna.library.path} to that
     * directory, and (on Windows) prepend it to the current process PATH so
     * that dependent DLLs (e.g. Boost) can be resolved by the OS loader.
     *
     * <p>Must be called <b>before</b> any JNA interface is first accessed,
     * because JNA reads {@code jna.library.path} at library-load time.</p>
     *
     * @return the temporary directory path where libraries were extracted
     * @throws IOException if extraction fails
     */
    public static synchronized Path extractNativeLibraries() throws IOException {
        if (nativeTempDir != null && Files.isDirectory(nativeTempDir)) {
            return nativeTempDir;
        }

        // 1. Create a stable temp directory
        nativeTempDir = Files.createTempDirectory("cammon_native");
        nativeTempDir.toFile().deleteOnExit();
        System.out.println("NativeLoader: extracting native libraries to " + nativeTempDir.toAbsolutePath());

        // 2. Determine the resource base path inside the JAR
        //    Try /native/ first (the standard packaging location)
        extractResourceDir("/native/", nativeTempDir);

        // 3. Also try /win32-x86-64/ if present (JNA convention)
        Path winDir = nativeTempDir;
        extractResourceDir("/win32-x86-64/", winDir);

        // 4. Set jna.library.path so JNA can find cammon.dll
        System.setProperty("jna.library.path", nativeTempDir.toAbsolutePath().toString());
        System.out.println("NativeLoader: jna.library.path = " + nativeTempDir.toAbsolutePath());

        // 5. On Windows, since all DLLs (including Boost dependencies) are
        //    extracted into the same temp directory, the OS DLL search order
        //    will find them next to cammon.dll automatically — no PATH
        //    modification needed.  We set a system property as a diagnostic
        //    hint so callers can discover the extraction directory if needed.
        System.setProperty("cammon.native.dir", nativeTempDir.toAbsolutePath().toString());

        return nativeTempDir;
    }

    /**
     * Extract all resources found under the given classpath directory
     * into the target filesystem directory.
     *
     * <p>When running from a JAR, classloader resource listing is limited,
     * so we fall back to a known list of library names.</p>
     */
    private static void extractResourceDir(String resourceDir, Path targetDir) throws IOException {
        // Known list of native libraries that may be packaged.
        // This ensures discovery even when running from a JAR where
        // ClassLoader.getResources() may not enumerate entries.
        String[] knownLibraries = {
            "cammon.dll",
            "boost_atomic-vc142-mt-x64-1_82.dll",
            "boost_date_time-vc142-mt-x64-1_82.dll",
            "boost_filesystem-vc142-mt-x64-1_82.dll",
            "boost_system-vc142-mt-x64-1_82.dll",
            "libcammon.so",
            "libboost_filesystem.so",
            "libboost_system.so"
        };

        for (String libName : knownLibraries) {
            String resourcePath = resourceDir + libName;
            InputStream is = NativeLoader.class.getResourceAsStream(resourcePath);
            if (is != null) {
                Path target = targetDir.resolve(libName);
                Files.copy(is, target, StandardCopyOption.REPLACE_EXISTING);
                is.close();
                target.toFile().deleteOnExit();
                System.out.println("NativeLoader: extracted " + resourcePath + " -> " + target.toAbsolutePath());
            }
        }
    }

    /**
     * Extract a single native library from the JAR and load it via
     * {@code System.load()}.  Suitable for JNI-style loading where
     * JNA is not used.
     *
     * @param resourcePath classpath resource path, e.g. "/native/cammon.dll"
     * @param tmpPrefix    prefix for the temporary file name
     * @throws IOException if the resource cannot be found or extraction fails
     */
    public static void loadFromJar(String resourcePath, String tmpPrefix) throws IOException {
        InputStream is = NativeLoader.class.getResourceAsStream(resourcePath);
        if (is == null) throw new IOException("Resource not found: " + resourcePath);
        
        Path tmp = Files.createTempFile(tmpPrefix, resourcePath.endsWith(".dll") ? ".dll" : null);
        tmp.toFile().deleteOnExit();
        Files.copy(is, tmp, StandardCopyOption.REPLACE_EXISTING);
        is.close();

        // Copy dependencies to the same temp directory
        String[] dependencies = {
            "boost_atomic-vc142-mt-x64-1_82.dll",
            "boost_date_time-vc142-mt-x64-1_82.dll",
            "boost_filesystem-vc142-mt-x64-1_82.dll",
            "boost_system-vc142-mt-x64-1_82.dll"
        };

        for (String dep : dependencies) {
            InputStream depIs = NativeLoader.class.getResourceAsStream("/native/" + dep);
            if (depIs != null) {
                Path depTmp = tmp.resolveSibling(dep);
                Files.copy(depIs, depTmp, StandardCopyOption.REPLACE_EXISTING);
                depIs.close();
                depTmp.toFile().deleteOnExit();
                System.out.println("NativeLoader: copied dependency: " + dep + " -> " + depTmp.toAbsolutePath());
            }
        }
        
        System.out.println("NativeLoader: loading " + resourcePath + " from " + tmp.toAbsolutePath());
        System.load(tmp.toAbsolutePath().toString());
    }
}
