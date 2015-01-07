package se.lth.cs.firefly;

import se.lth.control.labcomm.*;

import java.io.IOException;

public class Channel {
	private static final int INIT				= 1;
	private static final int OPEN				= 2;
	private static final int CLOSE_REQ_SENT		= 3;
	private static final int CLOSED				= 4;
	private static final int ERROR				= -1;

	private int state;
	private Encoder encoder;
	private ChannelDecoder decoder;
	private Connection conn;

	private int localID;
	private int remoteID;

	public Channel(int localID, Connection conn) {
		state = INIT;
		this.localID = localID;
		this.conn = conn;
	}

	public Encoder getEncoder()	{ return encoder;  }
	public Decoder getDecoder()	{ return decoder;  }
	public int     getLocalID()	{ return localID;  }
	public int     getRemoteID()	{ return remoteID; }

	public void receivedData(byte[] data) throws Exception {
		decoder.decodeData(data);
	}

	public boolean isOpen() { return state == OPEN; }
	public boolean isClosed() { return state == CLOSED; }

	public void setOpen()   { this.state = OPEN;   }
	public void setClosed() { this.state = CLOSED; }
	public void setError()  { this.state = ERROR;  }

	public void close() throws IOException {
		this.conn.closeChannel(this);
		this.state = CLOSE_REQ_SENT;
	}

}
