package se.lth.cs.firefly;

public interface FireflyServer extends FireflyApplication {
	// Incoming connections.
	public boolean acceptConnection(String hostAddress);
	public void connectionOpened();


}
