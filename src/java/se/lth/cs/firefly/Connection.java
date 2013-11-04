package se.lth.cs.firefly;

import se.lth.control.labcomm.*;

import java.util.ArrayList;

public class Connection {
	// private static final int INIT	= 1;
	// private static final int OPEN	= 2;
	// private static final int CLOSED = 3;
	// private static final int ERROR	= -1;

	// private int state;
	private LabCommEncoderChannel topEncoder;
	private LabCommDecoderChannel topDecoder;
	private ArrayList<Channel> channels;
	private long nextChannelID;

	public Connection() {
		// state = INIT;
		channels = new ArrayList<Channel>();
	}

	public void openChannel() {

	}

	public LabCommDecoderChannel getDecoder() { return topDecoder; }
	public LabCommEncoderChannel getEncoder() { return topEncoder; }
}
