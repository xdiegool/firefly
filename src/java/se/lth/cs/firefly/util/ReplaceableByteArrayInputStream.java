package se.lth.cs.firefly.util;

import java.io.ByteArrayInputStream;

/**
 * A subclass of {@link java.io.ByteArrayInputStream} that allows
 * the underlying buffer to be replaced.
 *
 * @author Erik Jansson<erikjansson90@gmail.com>
 */
public class ReplaceableByteArrayInputStream extends ByteArrayInputStream {

    public ReplaceableByteArrayInputStream(byte[] buf) {
        super(buf);
    }

    /**
     * Replaces the buffer that backs the InputStream.
     *
     * @param buf The buffer to replace the existing one with.
     */
    public void replace(byte[] buf) {
        this.buf = buf;
        reset();
    }
}
