package se.lth.cs.firefly;

import genproto.*;
import se.lth.control.labcomm.*;

import java.io.IOException;
import java.util.*;

/*
 * Handles channel state, encoder/decoder access and data ordering
 *	
 */
public class Channel {
	private static final int INIT = 1;
	private static final int OPEN = 2;
	private static final int CLOSE_REQ_SENT = 3;
	private static final int CLOSED = 4;
	private static final int ERROR = -1;

	private int state;
	private ChannelEncoder encoder;
	private ChannelDecoder decoder;
	private Connection conn;
	private boolean restricted;

	private int localID;
	private int remoteID;

	public Channel(int localID, Connection conn) {
		state = INIT;
		this.localID = localID;
		this.conn = conn;
		restricted = false;
	}

	public void setEncoder(ChannelEncoder e) {
		encoder = e;
	}

	public void setDecoder(ChannelDecoder d) {
		decoder = d;
	}

	public ChannelEncoder getEncoder() {
		return encoder;
	}

	public ChannelDecoder getDecoder() {
		return decoder;
	}

	public int getLocalID() {
		return localID;
	}

	public int getRemoteID() {
		return remoteID;
	}

	public void setRemoteID(int id) {
		remoteID = id;
	}

	public boolean isOpen() {
		return state == OPEN;
	}

	public boolean isClosed() {
		return state == CLOSED;
	}

	public boolean isRestricted() {
		return restricted;
	}

	public void setOpen() {
		this.state = OPEN;
	}

	public void setClosed() {
		this.state = CLOSED;
	}

	public void setError() {
		this.state = ERROR;
	}

	public void setRestricted(boolean b) {
		restricted = b;
	}

	public void close() throws IOException {
		this.conn.closeChannel(this);
		this.state = CLOSE_REQ_SENT;
	}

}
