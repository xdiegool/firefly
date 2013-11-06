package se.lth.cs.firefly;

import se.lth.control.labcomm.*;

// import java.io.IOException;

class Channel {
	private LabCommEncoder encoder;
	private ChannelDecoder decoder;

	public LabCommEncoder getEncoder() {
		return encoder;
	}

	public LabCommDecoder getDecoder() {
		return decoder;
	}

	public void receivedData(byte[] data) throws Exception {
		decoder.decodeData(data);
	}

}
