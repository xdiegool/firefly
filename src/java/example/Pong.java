package example;

import se.lth.cs.firefly.*;

public class Pong implements Runnable, FireflyServer {

	// Callbacks
	public void channelOpened() {}
	public void channelClosed() {}
	public void channelRestrict() {}
	public void channelStatus() {}
	public void channelError() {}
	public void connectionError() {}

	// More callbacks
	public boolean channelAccept() { return true; }
	public void connectionOpened() {}


	public void run() {
		TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this, 8080);
	}
}
