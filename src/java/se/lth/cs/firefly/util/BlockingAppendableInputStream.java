package se.lth.cs.firefly.util;

import java.io.*;

/**
 * Enhances the functionality of AppendableInputStream by providing blocking on
 * empty stream.
 */
public class BlockingAppendableInputStream extends AppendableInputStream {

	public BlockingAppendableInputStream(byte[] data) {
		super(data);
	}

	public BlockingAppendableInputStream() {
		super();
	}

	@Override
	public synchronized int read() throws IOException {
		while (bytesLeft == 0) {
			try {
				wait();
			} catch (InterruptedException e) {
				Thread.currentThread().interrupt();
				throw new InterruptedIOException(
						"Interrupted while waiting for bytes in stream");
			}

		}

		int a = super.read();
		Debug.log("Bytesleft: " + bytesLeft + " just read " + a);
		return a;
	}

	@Override
	public synchronized void append(byte[] data) throws IOException {
		super.append(data);
		notifyAll();
	}
}
