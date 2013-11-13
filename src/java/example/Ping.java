package example;

import se.lth.cs.firefly.*;

import se.lth.control.labcomm.LabCommEncoder;
import se.lth.control.labcomm.LabCommDecoder;

import lc_gen.data;

import java.io.IOException;

public class Ping
	implements Runnable, FireflyApplication, data.Handler
{
	// private boolean connOpen;
	// private boolean chanOpen;
	private Channel chan;
	private Connection conn;
	private int echo = -1;

	// Callbacks

	// TODO: Enforce convention regarding channel set up in
	//       client-server scenario?
	public boolean channelAccept(Connection connection) { return false; }
	public void channelOpened(Channel chan) { setChan(chan); }
	public void channelClosed(Channel chan) { setChan(null); }
	public void channelRestrict(Channel chan) {} // Not used.
	public void channelStatus(Channel chan) {}	 // Not used.
	public void channelError(Channel chan) { Debug.errx("Chan. err."); }
	public void connectionError(Connection conn) { Debug.errx("Conn. err."); }


	public void run() {
		try {
			reallyRun();
		} catch (InterruptedException e) {
		} catch (IOException e) {
		}
	}

	private synchronized void setChan(Channel chan) {
		this.chan = chan;
		notifyAll();
	}

	private synchronized void waitForChan() throws InterruptedException {
		while (this.chan == null) wait();
	}

	private synchronized void waitForEcho() throws InterruptedException {
		while (this.echo == -1) wait();
	}


	public synchronized void handle_data(int value) {
		System.out.println("Got data:" + value);
		notifyAll();
	}

	private void reallyRun() throws InterruptedException, IOException {
		TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this);
		Connection conn = connMux.openConnection(null, 8080); // Loopback
		conn.openChannel();
		waitForChan();
		LabCommEncoder enc = this.chan.getEncoder();
		LabCommDecoder dec = this.chan.getDecoder();
		data.register(enc);
		data.register(dec, this); // Reg. handler above.
		data.encode(enc, 123);
		waitForEcho();
		this.chan.close();

	}
	public static void main(String[] args) {
		new Ping().run();
	}

}
