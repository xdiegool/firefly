package example;

import se.lth.cs.firefly.*;

import se.lth.control.labcomm.LabCommEncoder;
import se.lth.control.labcomm.LabCommDecoder;
import java.net.*;
import lc_gen.data;

import java.io.IOException;

public class Pong implements Runnable, FireflyApplication, data.Handler {

	// Callbacks
	public boolean channelAccept(Connection connection) { return true; }
	public void channelOpened(Channel chan) { setChan(chan); }
	public void channelClosed(Channel chan) {}
	public void channelRestrict(Channel chan) {}
	public void channelStatus(Channel chan) {}
	public void channelError(Channel chan) {}
	public void connectionError(Connection conn) {}

	// More callbacks
	public boolean acceptConnection(InetAddress remoteAddress, int remotePort){return true;}
	public void connectionOpened(Connection conn){}


	public void run() {
		try {
			reallyRun();
		} catch (IOException e) {
			e.printStackTrace();	
			Debug.errx("IO broke: " + e);
		} catch (InterruptedException e) {
			e.printStackTrace();	
		}
	}

	private Channel chan;
	private int echo = -1;

	private synchronized void setChan(Channel chan) {
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
		//TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this, 8080);
		Connection conn = new UDPConnection(8056, this); 
		Debug.log("Waiting for channel...");		
		waitForChannel();
		Debug.log("Got chan");
		LabCommDecoder dec = chan.getDecoder();
		LabCommEncoder enc = chan.getEncoder();
		data.register(enc);
		data.register(dec, this); // Reg. handler above.;
		waitForData();
		Debug.log("Got data: " + echo);
		conn.close();

	}

	public static void main(String[] args) {
		new Pong().run();
	}
}
