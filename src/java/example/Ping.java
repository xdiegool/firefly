package example;

import se.lth.cs.firefly.*;
import se.lth.cs.firefly.protocol.*;
import se.lth.cs.firefly.transport.*;
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

	// Callbacks
	public boolean channelAccept(Connection connection) { return false; }
	public void channelOpened(Channel chan) { setChan(chan); }
	public void channelClosed(Channel chan) { setChan(null); }
	public void channelStatus(Channel chan) {}	 // Not used.
	public void channelError(Channel chan) { Debug.errx("Chan. err."); }
	public void connectionError(Connection conn) { Debug.errx("Conn. err."); }
	public boolean acceptConnection(InetAddress remoteAddress, int remotePort){ return true;}
	public void connectionOpened(Connection conn){}
	public boolean restrictAccept(Channel chan){ return restrict; }
	public void channelRestricted(Channel chan){ restricted(); }
	public void LLPError(LinkLayerPort p, Exception e) { Debug.errx("LinkLayerPort error "); }

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
		Debug.log("Ping: Got data:" + value);
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
		Debug.log("Ping: Regeistered decoder");
		Debug.log("Ping: Registering encoder");
		data.register(enc);
		Debug.log("Ping: Registered encoder");
		Debug.log("Pong: Restricting");
		conn.restrictChannel(chan);
		Debug.log("Ping: Waiting for restrict");
		waitForRestrict();
		Debug.log("Ping: Gto restrict");
		Debug.log("Ping: Sending data");
		data.encode(enc, 123);
		Debug.log("Ping: Sent data: 123, waiting for echo");
		waitForEcho();
		Debug.log("Ping: Echo received : " + (chan == null));
		connMux.close();
		queue.stop();
	}
	public static void main(String[] args) {
		new Ping().run();
	}


}
