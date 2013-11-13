package se.lth.cs.firefly;

import se.lth.control.labcomm.*;

// import java.io.IOException;

public class Channel {
	private static final int INIT	= 1;
	private static final int OPEN	= 2;
	private static final int CLOSED = 3;
	private static final int ERROR	= -1;

	private int state;
	private LabCommEncoder encoder;
	private ChannelDecoder decoder;

	private int localID;

	public Channel(int localID) {
		state = INIT;
		this.localID = localID;
	}

	public LabCommEncoder getEncoder() {
		return encoder;
	}

	public LabCommDecoder getDecoder() {
		return decoder;
	}

	public void receivedData(byte[] data) throws Exception {
		decoder.decodeData(data);
	}

	public boolean isOpen() { return state == OPEN; }
	public boolean isClosed() { return state == CLOSED; }

	public void setOpen()   { this.state = OPEN;   }
	public void setClosed() { this.state = CLOSED; }
	public void setError()  { this.state = ERROR;  }

}
