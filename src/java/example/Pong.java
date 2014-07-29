package example;

import se.lth.cs.firefly.*;

import se.lth.control.labcomm.LabCommEncoder;
import se.lth.control.labcomm.LabCommDecoder;
import java.net.InetAddress;
import lc_gen.data;

import java.io.IOException;

public class Pong implements Runnable, FireflyApplication, data.Handler {

	// Callbacks
	public boolean channelAccept(Connection connection) { return true; }
	public void channelOpened(Channel chan) { setChan(chan); }
	public void channelClosed(Channel chan) {}
	public void channelStatus(Channel chan) {}
	public void channelError(Channel chan) {}
	public void connectionError(Connection conn) {}
	public boolean restrictAccept(Channel chan){ return true; }
	public void channelRestricted(Channel chan){ restricted(); }

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
	private boolean chanNotRestricted = true;

	private synchronized void setChan(Channel chan) {
		this.chan = chan;
		notifyAll();
	}
	private synchronized void restricted(){
		chanNotRestricted = false;
		notifyAll();
	}
	private synchronized void waitForRestrict() throws InterruptedException{
		while(chanNotRestricted){wait();}
	}

	private synchronized void waitForChannel() throws InterruptedException {
		while (chan == null) wait();
	}

	public synchronized void handle_data(int value) throws IOException {
		Debug.log("Sending data: " + value);
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
conn.setDataAck(true);
		Debug.log("Waiting for channel...");		
		waitForChannel();
		Debug.log("Got chan");
		LabCommDecoder dec = chan.getDecoder();
		LabCommEncoder enc = chan.getEncoder();
		data.register(enc);
		data.register(dec, this); // Reg. handler above.;
		Debug.log("Restricting");
		conn.restrictChannel(chan);
		Debug.log("Waiting for restrict");
		waitForRestrict();
		Debug.log("Waiting for data");
		waitForData();
		Debug.log("Got data: " + echo);
		conn.close();

	}

	public static void main(String[] args) {
		new Pong().run();
	}
}
