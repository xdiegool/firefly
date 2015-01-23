package se.lth.cs.firefly.transport;

import se.lth.cs.firefly.util.Debug;

import se.lth.control.labcomm.DecoderChannel;
import se.lth.control.labcomm.SampleDispatcher;

import java.io.InputStream;
import java.io.IOException;

class ConnectionDecoder extends DecoderChannel {

	public ConnectionDecoder(InputStream in) throws IOException {
		super(in);
	}

	public void shortCircuit(int index, SampleDispatcher d) throws IOException {
		def_registry.add(index, d.getName(), d.getSignature());
	}
}
