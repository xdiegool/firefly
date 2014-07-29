package se.lth.cs.firefly;

import se.lth.control.labcomm.*;
import genproto.*;
import java.util.*;
import java.io.*;
import java.net.*;

/**
* This class handles the channel protocol and is
* a intermediary for passing data from remote host
* to local channels.
*
*/
public class UDPConnection extends Connection{ 

	
	private DatagramSocket dsock;
	

	public UDPConnection(int localPort, FireflyApplication delegate) throws IOException{	
		super(localPort, delegate);
		
	}	
	public UDPConnection(int localPort, int remotePort, InetAddress remoteAddress, FireflyApplication delegate) throws IOException{
		super(localPort, remotePort, remoteAddress, delegate);	
	}
	@Override
	protected void init() throws IOException{
		dsock = new DatagramSocket(localPort);
	}
	@Override
	protected int bufferSize()  throws IOException{
		return 64000;
	}
	@Override
	protected int receive(byte[] inc, int len)  throws IOException{
		DatagramPacket p = new DatagramPacket(inc, len);	
		dsock.receive(p);
		Debug.log("Received packet: " +Debug.byteArrayToString(p.getData(), p.getLength()));
		if(remoteAddress == null && delegate.acceptConnection(p.getAddress(), p.getPort())){
Debug.log("Delegate accepted");
			remoteAddress = p.getAddress(); //TODO Ask delegate if they accept packages from this host
			remotePort = p.getPort();
			startSending();
		}
		
		return p.getLength();
	}
	@Override
	protected void send(byte[] data, int length)  throws IOException{
		dsock.send(new DatagramPacket(data, length, remoteAddress,  remotePort));
		Debug.log("Sent packet: " +Debug.byteArrayToString(data, length));
	}
	@Override
	protected void closeTransport(){
		dsock.close();
	}
}
