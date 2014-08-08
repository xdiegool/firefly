package example;

import se.lth.cs.firefly.protocol.*;
import se.lth.cs.firefly.transport.UDPConnectionMultiplexer;
import se.lth.cs.firefly.util.ActionQueue;
import se.lth.cs.firefly.util.Debug;
import se.lth.control.labcomm.*;

import java.net.InetAddress;

import lc_gen.data;

import java.io.IOException;

public class Pong implements Runnable, FireflyApplication, data.Handler {

	// Accept requests
	public boolean channelAccept(Connection connection) { return true; }
	public boolean restrictAccept(Channel chan){ return true; }
	public boolean connectionAccept(InetAddress remoteAddress, int remotePort){return true;}
	// State change
	public void channelOpened(Channel chan) { setChan(chan); }
	public void channelClosed(Channel chan) {}
	public void connectionOpened(Connection conn) {}
	@Override
	public void channelRestrictStateChange(Channel chan, RestrictState rs) {
		if(rs.equals(RestrictState.RESTRICTED)){
			restricted();
		}
	}

	// Error callbacks
	public void channelError(Channel chan, String message, Exception e) {}
	public void connectionError(Connection conn, String message, Exception e) {}

	
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
	private Connection conn;

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
		Debug.log("Pong: Got data, sending: " + value);
		data.encode(chan.getEncoder(), value);
		this.echo = value;
		notifyAll();
	}

	private synchronized void waitForData() throws InterruptedException {
		while (echo == -1) wait();
	}

	private void reallyRun() throws IOException, InterruptedException {
		ActionQueue queue = new ActionQueue();
		UDPConnectionMultiplexer connMux = new UDPConnectionMultiplexer(8456, this, queue);
		Debug.log("Pong: Waiting for channel...");		
		waitForChannel();
		Debug.log("Pong: Got chan");
		chan.setAckOnData(true);
		LabCommDecoder dec = chan.getDecoder();
		LabCommEncoder enc = chan.getEncoder();
		Debug.log("Pong: Registering decoder");
		data.register(dec, this); // Reg. handler above.;
		Debug.log("Pong: Regeistered decoder");
		Debug.log("Pong: Registering encoder");
		data.register(enc);
		Debug.log("Pong: Registered encoder");
		Debug.log("Pong: Waiting for restrict");
		waitForRestrict();
		Debug.log("Pong: Done restricting");
		Debug.log("Pong: Waiting for data");
		waitForData();
		Debug.log("Pong: closing");
		connMux.close();
		queue.stop();
	}

	public static void main(String[] args) {
		new Pong().run();
	}


}
