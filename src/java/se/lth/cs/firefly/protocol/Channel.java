package se.lth.cs.firefly.protocol;

import se.lth.cs.firefly.transport.Connection;
import se.lth.cs.firefly.util.Debug;

import se.lth.control.labcomm.Constant;
import se.lth.control.labcomm.Decoder;
import se.lth.control.labcomm.Encoder;
import se.lth.control.labcomm.EncoderChannel;
import se.lth.control.labcomm.Writer;

import java.io.IOException;

public class Channel {
	public static final int ID_NOT_SET = -1;

	/**
	 * The current state of the channel. Starts as Channel.State.INIT.
	 */
	private State state;

	/**
	 * This channel's encoder to send data on.
	 */
	private Encoder encoder;

	/**
	 * This channel's decoder to receive data on.
	 */
	private ChannelDecoder decoder;

	/**
	 * The connection this channel exists on.
	 */
	private Connection conn;

	/**
	 * The local channel ID of this channel.
	 */
	private int localId;

	/**
	 * The remote channel ID of this channel.
	 */
	private int remoteId;

	/**
	 * Whether the remote end of this channel is restricted.
	 */
	private boolean remoteRestricted;

	/**
	 * Whether the local end of this channel is restricted.
	 */
	private boolean localRestricted;

	/**
	 * The current sequence number for this channel.
	 */
	private int sequenceNumber;

	/**
	 * The list of types to be registered on this channel's decoder and encoder.
	 */
	private TypeList channelTypes;

	/**
	 * TODO: Doc
	 */
	private int numRegisteredTypes;

	/**
	 * Debug logger for nice logging output.
	 */
	private Debug logger;

	/**
	 * Creates a new channel with the provided local channel ID and on the
	 * provided connection.
	 *
	 * NOTE: If this constructor is used, there must be a call to ...
	 */
	public Channel(int localId, Connection conn) {
		this(localId, -1, conn);
	}

	public Channel(int localId, int remoteId, Connection conn) {
		this.conn = conn;
		this.localId = localId;

		state = State.INIT;
		remoteId = ID_NOT_SET;
		remoteRestricted = false;
		localRestricted = false;
		numRegisteredTypes = 0;
		remoteId = ID_NOT_SET;
		sequenceNumber = 0;
		logger = new Debug(Channel.class.getCanonicalName());
	}

	/**
	 * Set the LabComm types to be used on this channel.
	 *
	 * @param types The list of types to use on this channel.
	 */
	public void setChannelTypes(TypeList types) {
		channelTypes = types;

		try {
			decoder = new ChannelDecoder();
			channelTypes.register(decoder);
		} catch (IOException e) {
			logger.logc("Failed while creating encoder/decoder: " + e.toString());
		}
	}

	public Connection getConnection() {
		return conn;
	}

	public Encoder getEncoder() {
		return encoder;
	}

	public Decoder getDecoder() {
		return decoder;
	}

	public int getLocalId() {
		return localId;
	}

	public void setRemoteId(int remoteId) {
		this.remoteId = remoteId;
	}

	public int getRemoteId() {
		return remoteId;
	}

	public boolean isRestricted() {
		return isRemotelyRestricted() && isLocallyRestricted();
	}

	public boolean isRemotelyRestricted() {
		return localRestricted;
	}

	public void setRemoteRestricted(boolean restricted) {
		remoteRestricted = restricted;
	}

	public boolean isLocallyRestricted() {
		return localRestricted;
	}

	public void setLocalRestricted(boolean restricted) {
		localRestricted = restricted;
	}

	public int getNextSequenceNumber() {
		return sequenceNumber++;
	}

	public void setOpen() {
		try {
			encoder = new ChannelEncoder(this);
			channelTypes.register(encoder);
			localRestricted = true;
		} catch (IOException e) {
			logger.logc("Failed while creating encoder/decoder: " + e.toString());
		}

		/* TODO: Auto register. */

		state = State.OPEN;
	}

	public boolean isOpen() {
		return state == State.OPEN;
	}

	public void setClosing() {
		state = State.CLOSE_REQ_SENT;
	}

	public boolean isClosing() {
		return state == State.CLOSE_REQ_SENT;
	}

	public void setClosed() {
		state = State.CLOSED;
	}

	public boolean isClosed() {
		return state == State.CLOSED;
	}

	public void setError() {
		state = State.ERROR;
	}

	public void close() throws IOException {
		conn.closeChannel(this);
		state = State.CLOSE_REQ_SENT;
	}

	public void receivedData(byte[] data) throws Exception {
		// logger.logc("receivedData(): " + String.format("0x%x...", data[0]));
		if (data[0] == Constant.SAMPLE_DEF) {
			if (++numRegisteredTypes == decoder.getNumDecoderTypes()) {
				conn.channelAutoRestricted(this);
			}
		}
		decoder.decodeData(data);
	}

	/**
	 * Enum representing the state of channels.
	 */
	private enum State {
		INIT,
		OPEN,
		CLOSE_REQ_SENT,
		CLOSED,
		ERROR
	}
}
