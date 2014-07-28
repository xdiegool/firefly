package se.lth.cs.firefly;

import java.net.*;

public interface FireflyApplication {
	// Reduce.
	// Want blocking ops?
	public boolean channelAccept(Connection conn);
	public void channelOpened(Channel chan);
	public void channelClosed(Channel chan);
	public void channelRestrict(Channel chan);
	public void channelStatus(Channel chan);
	public void channelError(Channel chan);
	public void connectionError(Connection conn);

	// Incoming connections.
	public boolean acceptConnection(InetAddress remoteAddress, int remotePort);
	public void connectionOpened(Connection conn);

}
