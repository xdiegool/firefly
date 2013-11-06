package se.lth.cs.firefly;

import se.lth.control.labcomm.*;

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;

class ChannelDecoder extends LabCommDecoderChannel {
	// Used to decode the datagram unpacked from an firefly data_sample.
	public ChannelDecoder() throws IOException {
		 // Bypass version assert.
		super(new ByteArrayInputStream(LabComm.VERSION.getBytes()));
	}

	public void decodeData(byte[] data) throws Exception {
		this.in = new DataInputStream(new ByteArrayInputStream(data));
		runOne();
	}
}
