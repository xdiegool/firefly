package se.lth.cs.firefly;

import se.lth.control.labcomm.LabCommDecoderChannel;
import se.lth.control.labcomm.LabCommDispatcher;

import java.io.InputStream;
import java.io.IOException;

class ConnectionDecoder extends LabCommDecoderChannel {

	public ConnectionDecoder(InputStream in) throws IOException {
		super(in);
	}

	public void shortCircuit(int index, LabCommDispatcher d)
		throws IOException
	{
		registry.add(index, d.getName(), d.getSignature());
	}
}
