package se.lth.cs.firefly.protocol;

import java.net.InetAddress;

/**
* Interface for callbacks to an application using firefly.
*
*/
public interface FireflyApplication {
	// Application accept request
	public boolean channelAccept(Connection conn); // TODO Add channel here as well? 
	public boolean restrictAccept(Channel chan);
	public boolean connectionAccept(InetAddress remoteAddress, int remotePort);
	
	// Channel state changes
	public void channelOpened(Channel chan);
	public void channelClosed(Channel chan);
	public void channelRestrictStateChange(Channel chan, RestrictState rs);
	public void connectionOpened(Connection conn);

	
	// Error handling
	// public void LLPError(LinkLayerPort llp, String message, Exception e); // Not used atm
	public void connectionError(Connection conn, String message, Exception e);
	public void channelError(Channel chan, String message, Exception e);
	
	public enum RestrictState {
		RESTRICTED, UNRESTRICTED, RESTRICT_REFUSED;
	}
}
