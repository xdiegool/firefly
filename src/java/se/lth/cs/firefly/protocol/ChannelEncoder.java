package se.lth.cs.firefly.protocol;

import se.lth.control.labcomm.*;

import java.io.*;

class ChannelEncoder extends LabCommEncoderChannel {
	// Used to decode the datagram unpacked from an firefly data_sample.
	public ChannelEncoder(LabCommWriter writer) throws IOException {
		super(writer,false);
	}
}
