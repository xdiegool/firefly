package se.lth.cs.firefly.protocol;


import se.lth.control.labcomm.*;

import java.io.OutputStream;
import java.io.IOException;

public class ConnectionEncoder extends LabCommEncoderChannel {
	private ConnectionDecoder dec;
	private LabCommEncoderRegistry registry;
	public ConnectionEncoder(LabCommWriter writer)
		throws IOException
	{
		super(writer,true);
		registry = new LabCommEncoderRegistry();
		super.end(null);
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
	public void setShortCircuit(ConnectionDecoder bottomDecoder) {
		dec = bottomDecoder;
		
	}
	
}
