package example;

import se.lth.cs.firefly.protocol.*;
import se.lth.cs.firefly.transport.*;
import se.lth.cs.firefly.util.ActionQueue;
import se.lth.cs.firefly.util.Debug;
import se.lth.control.labcomm.*;

import java.net.InetAddress;

import lc_gen.data;

import java.io.IOException;

public class Ping implements Runnable, FireflyApplication, data.Handler
{
	// private boolean connOpen;
	// private boolean chanOpen;
	private Channel chan;
	private Connection conn;
	private int echo = -1;
	private boolean chanNotRestricted = true;
	private boolean restrict = false;

	// Accept requests
	public boolean channelAccept(Connection connection) { return false; }
	public boolean restrictAccept(Channel chan){ return restrict; }
	public boolean connectionAccept(InetAddress remoteAddress, int remotePort){ return true;}
	
	// State change
	public void channelOpened(Channel chan) { setChan(chan); }
	public void channelClosed(Channel chan) { setChan(null); }
	@Override
	public void connectionOpened(Connection conn) {}
	@Override
	public void channelRestrictStateChange(Channel chan, RestrictState rs) {
		if(rs.equals(RestrictState.RESTRICTED)){
			restricted();
		}
	}

	// Error callbacks
	public void channelError(Channel chan, String message, Exception e) {Debug.errx("Chan. err.");}
	public void connectionError(Connection conn, String message, Exception e) { Debug.errx("Conn. err."); }
	
	public void run() {
		try {
			reallyRun();
		} catch (InterruptedException e) {	
			e.printStackTrace();	
		} catch (IOException e) {
			e.printStackTrace();	
		}
	}

	private synchronized void setChan(Channel chan) {
		this.chan = chan;
		notifyAll();
	}
	private synchronized void restricted(){
		chanNotRestricted = false;
		notifyAll();
	}
	private synchronized void waitForRestrict() throws InterruptedException{
		restrict = true;
		while(chanNotRestricted){wait();}
	}

	private synchronized void waitForChan() throws InterruptedException {
		while (this.chan == null) wait();
	}

	private synchronized void waitForEcho() throws InterruptedException {
		while (this.echo == -1) wait();
	}


	public synchronized void handle_data(int value) {
		Debug.log("Ping: got data:" + value);
		echo = value;
		notifyAll();
	}

	private void reallyRun() throws InterruptedException, IOException {
		ActionQueue queue = new ActionQueue();
		UDPConnectionMultiplexer connMux = new UDPConnectionMultiplexer(this, queue);
		Connection conn = connMux.openConnection(InetAddress.getByName("localhost"), 8456); // Loopback
		conn.openChannel();
		Debug.log("Ping: Waiting for channel...");
		waitForChan();
		Debug.log("Ping: Got Channel");
		chan.setAckOnData(true);
		LabCommEncoder enc = this.chan.getEncoder();
		LabCommDecoder dec = this.chan.getDecoder();
		Debug.log("Ping: Registering decoder");
		data.register(dec, this); // Reg. handler above.;
		Debug.log("Ping: Registered decoder");
		Debug.log("Ping: Registering encoder");
		data.register(enc);
		Debug.log("Ping: Registered encoder");
		Debug.log("Pong: Restricting");
		conn.restrictChannel(chan);
		Debug.log("Ping: Waiting for restrict");
		waitForRestrict();
		Debug.log("Ping: Got restrict");
		Debug.log("Ping: Sending data");
		data.encode(enc, 123);
		Debug.log("Ping: Sent data: 123, waiting for echo");
		waitForEcho();
		Debug.log("Ping: Echo received");
		connMux.close();
		queue.stop();
	}
	public static void main(String[] args) {
		new Ping().run();
	}



}
