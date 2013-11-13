package se.lth.cs.firefly;

import se.lth.control.labcomm.*;

import java.util.HashMap;
import java.io.IOException;

public abstract class Connection {
	protected HashMap<Integer, Channel> channels;
	protected int nextChannelID;

	public Connection() {
		// state = INIT;
		channels = new HashMap<Integer, Channel>();
	}

	public abstract void openChannel() throws IOException;
	public abstract void closeChannel(Channel chan) throws IOException;
}
