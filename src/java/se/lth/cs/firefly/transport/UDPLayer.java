package se.lth.cs.firefly.transport;

import java.io.*;
import java.net.*;

import se.lth.control.labcomm.LabCommWriter;
import se.lth.cs.firefly.protocol.TransportLayerAbstraction;
import se.lth.cs.firefly.util.*;

public class UDPLayer implements TransportLayerAbstraction {
	private DatagramSocket dsock;
	private InetAddress remoteAddress;
	private int remotePort;
	private LayerWriter writer;
	private boolean closed;
	private InputStream is;

	public UDPLayer(DatagramSocket dsock, InetAddress remoteAddress,
			int remotePort, AppendableInputStream is) {
		this.dsock = dsock;
		this.remoteAddress = remoteAddress;
		this.remotePort = remotePort;
		writer = new LayerWriter();
		closed = false;
		this.is = is;
	}

	@Override
	public InputStream getInputStream() {
		return is;
	}

	@Override
	public LabCommWriter getWriter() {
		return writer;
	}

	@Override
	public synchronized void close() {
		closed = true;
	}
	public synchronized boolean isClosed(){
		return closed;
		
	}

	public class LayerWriter implements LabCommWriter {
		// All variables are thread specific, except socket, address and port,
		// which are only set at creation. DatagramSocket.send is atomic so this
		// method shouldn't need synchronization.
		public void write(byte[] data) throws IOException {
			if (isClosed()) {
				throw new SocketException("Socket is closed");
			}
			dsock.send(new DatagramPacket(data, data.length, remoteAddress,
					remotePort));
			Debug.log("Sent packet: "
					+ Debug.byteArrayToString(data, data.length));
		}
	}

	@Override
	public InetAddress getRemoteHost() {
		return remoteAddress;
	}

	@Override
	public int getRemotePort() {
		return remotePort;
	}

}
