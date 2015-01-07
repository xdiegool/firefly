package example;

import se.lth.cs.firefly.*;

import se.lth.control.labcomm.Encoder;
import se.lth.control.labcomm.Decoder;

import lc_gen.data;

import java.io.IOException;

public class Pong implements Runnable, FireflyApplication, FireflyServer, data.Handler {

	// Callbacks
	public boolean channelAccept(Connection connection) { return true; }
	public void channelOpened(Channel chan) { setChan(chan); }
	public void channelClosed(Channel chan) {}
	public void channelRestrict(Channel chan) {}
	public void channelStatus(Channel chan) {}
	public void channelError(Channel chan) {}
	public void connectionError(Connection conn) {}

	// More callbacks
	public boolean acceptConnection(String addr) { return true; }
	public void connectionOpened() {}


	public void run() {
		try {
			reallyRun();
		} catch (IOException e) {
			Debug.errx("IO broke: " + e);
		} catch (InterruptedException e) {
		}
	}

	private Channel chan;
	private int echo = -1;

	private void setChan(Channel chan) {
		this.chan = chan;
		notifyAll();
	}

	private synchronized void waitForChannel() throws InterruptedException {
		while (chan == null) wait();
	}

	public synchronized void handle_data(int value) throws IOException {
		data.encode(chan.getEncoder(), value);
		this.echo = value;
		notifyAll();
	}

	private synchronized void waitForData() throws InterruptedException {
		while (echo == -1) wait();
	}

	private void reallyRun() throws IOException, InterruptedException {
		TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this, 8080);
		waitForChannel();
		System.out.println("Got chan");

        /* Get encoder and decoder from channel. */
		final Decoder dec = chan.getDecoder();
		final Encoder enc = chan.getEncoder();

		data.register(enc);
		data.register(dec, this); // Reg. handler above.
		waitForData();
		System.out.println("Got data");

	}

	public static void main(String[] args) {
		new Pong().run();
	}
}
