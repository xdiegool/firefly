package se.lth.cs.firefly.protocol;

import se.lth.control.labcomm.*;
import se.lth.cs.firefly.util.AppendableInputStream;

import java.io.*;

class ChannelDecoder extends LabCommDecoderChannel {
	private AppendableInputStream is;
	// Used to decode the datagram unpacked from an firefly data_sample.
	public ChannelDecoder(AppendableInputStream is) throws IOException {
		super(is);
		this.is= is;
	}

	public void decodeData(byte[] data) throws Exception {
		is.append(data);
		runOne();
	}
}
