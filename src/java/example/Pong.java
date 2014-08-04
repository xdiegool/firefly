package example;

import se.lth.cs.firefly.*;
import se.lth.cs.firefly.protocol.*;
import se.lth.cs.firefly.transport.UDPConnectionMultiplexer;
import se.lth.cs.firefly.util.Debug;
import se.lth.control.labcomm.*;

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
	public void LLPError(LinkLayerPort p, Exception e) { Debug.errx("LinkLayerPort error "); }
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
