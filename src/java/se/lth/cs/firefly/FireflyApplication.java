package se.lth.cs.firefly;

public interface FireflyApplication {
	// Reduce.
	// Want blocking ops?
	public void channelOpened();
	public void channelClosed();
	public void channelRestrict();
	public void channelStatus();
	public void channelError();

	public void connectionError();


}
