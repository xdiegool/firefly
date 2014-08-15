package se.lth.cs.firefly.protocol;

import se.lth.control.labcomm.*;

import java.io.*;

/**
* Only exists to limit the constructors of the superclass and to provide consistency in decoder/encoder pairs.
*/
class ChannelEncoder extends LabCommEncoderChannel {
	// Used to decode the datagram unpacked from an firefly data_sample.
	public ChannelEncoder(LabCommWriter writer) throws IOException {
		super(writer,false);
	}
}
