package se.lth.cs.firefly;

import java.net.InetAddress;

import se.lth.cs.firefly.protocol.Channel;
import se.lth.cs.firefly.protocol.Connection;
import se.lth.cs.firefly.protocol.LinkLayerPort;

/**
* Interface for callbacks to an application using firefly.
*
*/
public interface FireflyApplication {
	// Reduce.
	// Want blocking ops?
	public boolean channelAccept(Connection conn);
	public void channelOpened(Channel chan);
	public void channelClosed(Channel chan);
	public void channelStatus(Channel chan);
	public void channelError(Channel chan);
	public void connectionError(Connection conn);
	public boolean restrictAccept(Channel chan);
	public void channelRestricted(Channel chan);

	// Incoming connections.
	public boolean acceptConnection(InetAddress remoteAddress, int remotePort);
	public void connectionOpened(Connection conn);
	
	//LLP
	public void LLPError(LinkLayerPort llp, Exception e);

}
