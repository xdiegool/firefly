package se.lth.cs.firefly.protocol;

import se.lth.control.labcomm.*;

import java.io.*;

public class ConnectionDecoder extends LabCommDecoderChannel {
	private LabCommDecoderRegistry registry; //Since the superclass registry variable is private, we cannot access it and we have to create our own. 
	public ConnectionDecoder(InputStream inputStream) throws IOException {
		super(inputStream);
		registry = new LabCommDecoderRegistry();
	}
	public void shortCircuit(int index, LabCommDispatcher d) throws IOException {
		registry.add(index, d.getName(), d.getSignature());
	}
	/**
	* A direct copy of the corresponding super class method from the labcomm-core repo at 2014-07-25. If it has been updated since then,
	* this has to be updated as well. This copy was necessary for this method to access the subclass registry variable instead of
	* the superclass version of it. This in turn was necessary for the short circuit of type registration to work. 
	*/	
	@Override
	public void register(LabCommDispatcher dispatcher, 
                       LabCommHandler handler) throws IOException {
    	registry.add(dispatcher, handler);
  	}
	/**
	* Direct copy of the corresponding super class method, see above. 
	*
	*/
	@Override
	public void runOne() throws Exception {
		boolean done = false;
		while (!done) {
		  int tag = decodePacked32();
		  switch (tag) {
			case LabComm.TYPEDEF:
			case LabComm.SAMPLE: {
			  int index = decodePacked32();
			  String name = decodeString();
			  ByteArrayOutputStream signature = new ByteArrayOutputStream();
			  collectFlatSignature(new LabCommEncoderChannel(signature, false));
			  registry.add(index, name, signature.toByteArray());
			} break;
			default: {
			  LabCommDecoderRegistry.Entry e = registry.get(tag);
			  if (e == null) {
				throw new IOException("Unhandled tag " + tag);
			  }
			  LabCommDispatcher d = e.getDispatcher();
			  if (d == null) {
				throw new IOException("No dispatcher for '" + e.getName() + "'");
			  }
			  LabCommHandler h = e.getHandler();
			  if (h == null) {
				throw new IOException("No handler for '" + e.getName() +"'");
			  }
			  d.decodeAndHandle(this, h);
			  done = true;
			}
		  }
		}
  	}
	/**
	* Same as above.
	*/
	private void collectFlatSignature(LabCommEncoder out) throws IOException {
		int type = decodePacked32();
		out.encodePacked32(type);
		switch (type) {
		  case LabComm.ARRAY: {
		int dimensions = decodePacked32();
		out.encodePacked32(dimensions);
		for (int i = 0 ; i < dimensions ; i++) {
		  out.encodePacked32(decodePacked32());
		}
		collectFlatSignature(out);
		  } break;
		  case LabComm.STRUCT: {
		int fields = decodePacked32();
		out.encodePacked32(fields);
		for (int i = 0 ; i < fields ; i++) {
		  out.encodeString(decodeString());
		  collectFlatSignature(out);
		}
		  } break;
		  case LabComm.BOOLEAN:
		  case LabComm.BYTE:
		  case LabComm.SHORT:
		  case LabComm.INT:
		  case LabComm.LONG:
		  case LabComm.FLOAT:
		  case LabComm.DOUBLE:
		  case LabComm.STRING: {
		  } break;
		  default: {
		throw new IOException("Unimplemented type=" + type);
		  }
		}
		out.end(null);
  	}
	


}
