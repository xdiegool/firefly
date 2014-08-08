package se.lth.cs.firefly.util;
import java.io.*;
import java.net.*;

import se.lth.control.labcomm.LabCommWriter;

public interface TransportLayerAbstraction {
	
	public InputStream getInputStream() throws IOException ;
	public LabCommWriter getWriter() throws IOException ;
	public void close() throws IOException ;
	public InetAddress getRemoteHost();
	public int getRemotePort();
}
