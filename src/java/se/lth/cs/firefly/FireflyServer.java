package se.lth.cs.firefly;

import se.lth.cs.firefly.transport.Connection;

public interface FireflyServer extends FireflyApplication {
	// Incoming connections.
	public boolean acceptConnection(String hostAddress);
	public void connectionOpened(Connection conn);
}
