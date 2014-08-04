package se.lth.cs.firefly.transport;

import java.io.*;
import java.net.*;

import se.lth.control.labcomm.*;
import se.lth.cs.firefly.protocol.TransportLayerAbstraction;

public class TCPLayer implements TransportLayerAbstraction {
	private Socket socket;
	
	public TCPLayer(Socket socket){
		this.socket = socket;
	}

	@Override
	public InputStream getInputStream() throws IOException {
		return socket.getInputStream();
	}

	@Override
	public LabCommWriter getWriter() {
		return new LabCommWriter(){
			public void write(byte[] data) throws IOException{
				socket.getOutputStream().write(data);		
			}
		};
	}

	@Override
	public void close() throws IOException {
		socket.close();
	}

	@Override
	public InetAddress getRemoteHost() {
		return socket.getInetAddress();
	}

	@Override
	public int getRemotePort() {
		return socket.getPort();
	}

}
