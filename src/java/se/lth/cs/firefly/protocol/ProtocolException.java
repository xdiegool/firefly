package se.lth.cs.firefly.protocol;

import java.lang.IllegalStateException;

/**
 * Subclass of {@link java.lang.IllegalStateException} that is thrown
 * by Firefly if the protocol is in an erroneous state.
 */
@SuppressWarnings("serial")
public class ProtocolException extends IllegalStateException {
	public ProtocolException(String s) {
		super(s);
	}
}
