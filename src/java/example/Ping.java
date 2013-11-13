package example;

import se.lth.cs.firefly.*;

import java.io.IOException;

public class Ping implements Runnable, FireflyApplication {
	private boolean conn_open;
	private boolean chan_open;

	// Callbacks
	public boolean channelAccept(Connection connection) { return false; }
	public void channelOpened() {}
	public void channelClosed() {}
	public void channelRestrict() {}
	public void channelStatus() {}
	public void channelError() {}
	public void connectionError() {}
	public void channelClosed(Channel chan) {}


	public void run() {
		try {
			reallyRun();
		} catch (InterruptedException e) {
		} catch (IOException e) {
		}
	}

	private void reallyRun() throws InterruptedException, IOException {
		TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this);
		Connection conn = connMux.openConnection(null, 8080); // Loopback
		conn.openChannel();
		while (!chan_open) wait();

	}
}
