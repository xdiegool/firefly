package se.lth.cs.firefly;

import se.lth.control.labcomm.LabCommEncoderChannel;
import se.lth.control.labcomm.LabCommDispatcher;

import java.io.OutputStream;
import java.io.IOException;

class ConnectionEncoder extends LabCommEncoderChannel {
	ConnectionDecoder dec;	// Decoder to short circuit registration to.

	public ConnectionEncoder(OutputStream out, ConnectionDecoder dec)
		throws IOException
	{
		super(out);
		this.dec = dec;
	}

	public void register(LabCommDispatcher d) throws IOException
	{
		int index = registry.add(d);
		dec.shortCircuit(index, d);
	}
}
