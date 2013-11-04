package example;

import se.lth.cs.firefly.*;

public class Ping implements Runnable, FireflyApplication {
	private boolean conn_open;
	private boolean chan_open;

	// Callbacks
	public void channelOpened() {}
	public void channelClosed() {}
	public void channelRestrict() {}
	public void channelStatus() {}
	public void channelError() {}
	public void connectionError() {}


	public void run() {
		try {
			reallyRun();
		} catch (InterruptedException e) { }
	}

	private void reallyRun() throws InterruptedException {
		TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this);
		Connection conn = connMux.openConnection(null, 8080); // Loopback
		conn.openChannel();
		while (!chan_open) wait();

	}
}
