package example;

import se.lth.cs.firefly.*;

import java.io.IOException;

public class Pong implements Runnable, FireflyServer {

	// Callbacks
	public boolean channelAccept(Connection connection) { return true; }
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
		try {
			reallyRun();
		} catch (IOException e) {

		}
	}

	private void reallyRun() throws IOException {
		TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this, 8080);

	}
}
