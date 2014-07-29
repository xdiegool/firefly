package example;

import se.lth.cs.firefly.*;

import se.lth.control.labcomm.LabCommEncoder;
import se.lth.control.labcomm.LabCommDecoder;
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

	// Callbacks
	public boolean channelAccept(Connection connection) { return false; }
	public void channelOpened(Channel chan) { setChan(chan); }
	public void channelClosed(Channel chan) { setChan(null); }
	public void channelStatus(Channel chan) {}	 // Not used.
	public void channelError(Channel chan) { Debug.errx("Chan. err."); }
	public void connectionError(Connection conn) { Debug.errx("Conn. err."); }
	public boolean acceptConnection(InetAddress remoteAddress, int remotePort){ return true;}
	public void connectionOpened(Connection conn){}
	public boolean restrictAccept(Channel chan){ return true; }
	@Override
	public void channelRestricted(Channel chan){ restricted(); }

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
		while(chanNotRestricted){wait();}
	}

	private synchronized void waitForChan() throws InterruptedException {
		while (this.chan == null) wait();
	}

	private synchronized void waitForEcho() throws InterruptedException {
		while (this.echo == -1) wait();
	}


	public synchronized void handle_data(int value) {
		Debug.log("Got data:" + value);
		echo = value;
		notifyAll();
	}

	private void reallyRun() throws InterruptedException, IOException {
		//TCPConnectionMultiplexer connMux = new TCPConnectionMultiplexer(this);
		//Connection conn = connMux.openConnection(null, 8080); // Loopback
		Connection conn = new UDPConnection(8456, 8056, InetAddress.getByName("localhost"), this); 
		conn.setDataAck(true);
		conn.openChannel();
		Debug.log("Waiting for channel...");
		waitForChan();
		Debug.log("Got Channel");
		LabCommEncoder enc = this.chan.getEncoder();
		LabCommDecoder dec = this.chan.getDecoder();
		data.register(dec, this); // Reg. handler above.;
		data.register(enc);
		Debug.log("Restricting");
		conn.restrictChannel(chan);
		Debug.log("Waiting for restrict");
		waitForRestrict();
		Debug.log("Sending data");
		data.encode(enc, 123);
		Debug.log("Sent data: 123, waiting for echo");
		waitForEcho();
		Debug.log("Echo received");
		this.chan.close();
		conn.close();
	}
	public static void main(String[] args) {
		new Ping().run();
	}

}
