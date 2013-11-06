package se.lth.cs.firefly;

import se.lth.control.labcomm.*;

import java.util.HashMap;
import java.io.IOException;

public abstract class Connection {
	// private static final int INIT	= 1;
	// private static final int OPEN	= 2;
	// private static final int CLOSED = 3;
	// private static final int ERROR	= -1;

	// private int state;
	protected HashMap<Integer, Channel> channels;
	protected int nextChannelID;

	public Connection() {
		// state = INIT;
		channels = new HashMap<Integer, Channel>();
	}

	public abstract void openChannel() throws IOException ;
}
