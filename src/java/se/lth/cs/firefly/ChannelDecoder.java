package se.lth.cs.firefly;

import se.lth.cs.firefly.util.ReplaceableByteArrayInputStream;

import se.lth.control.labcomm.Constant;
import se.lth.control.labcomm.DecoderChannel;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;

class ChannelDecoder extends DecoderChannel {
	private ReplaceableByteArrayInputStream is;

	// Used to decode the datagram unpacked from an firefly data_sample.
	public ChannelDecoder() throws IOException {
		// Bypass version assert. TODO: What does this mean?
		this(new ReplaceableByteArrayInputStream(Constant.CURRENT_VERSION.getBytes()));
	}

	/*
	 * Private constructor which is needed to let us save the is parameter
	 * in this.is.
	 */
	private ChannelDecoder(ReplaceableByteArrayInputStream is) throws IOException {
		super(is);
		this.is = is;
	}

	public void decodeData(byte[] data) throws Exception {
		is.replace(data);
		runOne();
	}
}
