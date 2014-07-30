package se.lth.cs.firefly;


import se.lth.control.labcomm.*;

import java.io.OutputStream;
import java.io.IOException;

class ConnectionEncoder extends LabCommEncoderChannel {
	private ConnectionDecoder dec;
	private LabCommEncoderRegistry registry;
	public ConnectionEncoder(LabCommWriter writer, ConnectionDecoder dec)
		throws IOException
	{
		super(writer,false);
		this.dec = dec;
		registry = new LabCommEncoderRegistry();
	}
	@Override
	public void register(LabCommDispatcher d) throws IOException
	{
		int index = registry.add(d);
		dec.shortCircuit(index, d);
	}
	@Override
	 public void begin(Class<? extends LabCommSample> c) throws 	IOException {
		encodePacked32(registry.getTag(c));
  	}
	
}
