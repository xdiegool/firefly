package se.lth.cs.firefly.util;

import java.io.ByteArrayInputStream;

/**
 * A subclass of {@link java.io.ByteArrayInputStream} that allows
 * the underlying buffer to be replaced.
 *
 * @author Erik Jansson<erikjansson90@gmail.com>
 */
public class AppendableByteArrayInputStream extends ByteArrayInputStream {

	public AppendableByteArrayInputStream() {
		super(new byte[0]);
	}

	/**
	 * Appends data to the buffer that backs the InputStream.
	 *
	 * @param buf The buffer to append to the existing one.
	 */
	public void append(byte[] buf) {
		byte[] tmp = new byte[this.buf.length + buf.length];
		System.arraycopy(this.buf, 0, tmp, 0, this.buf.length);
		System.arraycopy(buf, 0, tmp, this.buf.length, buf.length);
		this.buf = tmp;
		this.count = this.buf.length;
	}

	public void clear() {
		reset();
		this.buf = new byte[0];
		this.count = 0;
	}
}
