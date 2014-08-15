package se.lth.cs.firefly.protocol;
import java.io.*;
import java.net.*;

import se.lth.control.labcomm.LabCommWriter;
/**
 * This interface abstracts the protocol specific stuff from Connection and Channel.
 * It is essentially a wrapper for the network protocol. 
 *
 */
public interface TransportLayerAbstraction {
	
	public InputStream getInputStream() throws IOException ;
	public LabCommWriter getWriter() throws IOException ;
	public void close() throws IOException ;
	public InetAddress getRemoteHost();
	public int getRemotePort();
}
