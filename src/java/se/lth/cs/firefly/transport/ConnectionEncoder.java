package se.lth.cs.firefly.transport;

import se.lth.cs.firefly.util.Debug;

import se.lth.control.labcomm.EncoderChannel;
import se.lth.control.labcomm.SampleDispatcher;

import java.io.OutputStream;
import java.io.IOException;

class ConnectionEncoder extends EncoderChannel {
	ConnectionDecoder dec;	// Decoder to short circuit registration to.

	public ConnectionEncoder(OutputStream out, ConnectionDecoder dec) throws IOException {
		super(out);
		this.dec = dec;
	}

	@Override
	public void register(SampleDispatcher d) throws IOException {
		int index = def_registry.add(d);
		dec.shortCircuit(index, d);
	}
}
