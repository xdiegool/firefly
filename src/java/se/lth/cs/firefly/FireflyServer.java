package se.lth.cs.firefly;

public interface FireflyServer extends FireflyApplication {
	public boolean channelAccept();
	// Incoming connections.
	public void connectionOpened();


}
