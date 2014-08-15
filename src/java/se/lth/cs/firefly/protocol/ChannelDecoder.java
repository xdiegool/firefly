package se.lth.cs.firefly.protocol;

import se.lth.control.labcomm.*;
import se.lth.cs.firefly.util.AppendableInputStream;
import se.lth.cs.firefly.util.Debug;

import java.io.*;

/**
 * The decoder for a channel, one instance per channel.
 * 
 */
class ChannelDecoder extends LabCommDecoderChannel {
	private AppendableInputStream is;

	// Used to decode the datagram unpacked from an firefly data_sample.
	public ChannelDecoder(AppendableInputStream is) throws IOException {
		super(is);
		this.is = is;
	}

	public void decodeData(byte[] data) throws Exception {
		is.append(data);
		try {
			runOne();
		} catch (EOFException e) {
			// Happens when user defined types are done
			// sending. If this happens, we only want to
			// continue. This happens because the decoders
			// can't handle non blocking IO on type registration.
			// TODO Fix this, possibly add thread in channel as well or let
			// Connection.Reader do everything.
		}
	}
}
