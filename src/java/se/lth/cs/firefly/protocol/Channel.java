package se.lth.cs.firefly.protocol;

import genproto.ack;

import java.io.IOException;
import java.util.LinkedList;
import java.util.Queue;

import se.lth.control.labcomm.LabComm;
import se.lth.control.labcomm.LabCommWriter;
import se.lth.cs.firefly.util.ActionQueue;
import se.lth.cs.firefly.util.Debug;
import se.lth.cs.firefly.util.ResendQueue;
import se.lth.cs.firefly.util.ActionQueue.Priority;

/*
 * Handles channel state and encoder/decoder access
 *	
 */
public class Channel {
	public static int CHANNEL_ID_NOT_SET = -1;
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
	private boolean remoteRestricted;
	private boolean ackOnData;
	private int localID;
	private int remoteID;
	private int remoteSeqno;
	private int currentSeqno;
	private Queue<byte[]> importantQueue;
	private ChannelWriter writer;
	private ActionQueue actionQueue;
	private ResendQueue resendQueue;

	public Channel(int localID, Connection conn, ActionQueue actionQueue, ResendQueue resendQueue) {
		state = INIT;
		this.actionQueue = actionQueue;
		this.resendQueue = resendQueue;
		this.localID = localID;
		this.remoteID = CHANNEL_ID_NOT_SET;
		this.conn = conn;
		restricted = false;
		remoteRestricted=false;
		ackOnData = false;
		remoteSeqno = 0;
		currentSeqno = 0;
		importantQueue = new LinkedList<>();
		writer = new ChannelWriter();

	}

	public synchronized void setEncoder(ChannelEncoder e) {
		encoder = e;
	}
	public synchronized void setDecoder(ChannelDecoder d) {
		decoder = d;
	}

	public synchronized ChannelEncoder getEncoder() {
		return encoder;
	}

	public ChannelDecoder getDecoder() {
		return decoder;
	}
	public synchronized void setAckOnData(boolean b){
		ackOnData = b;
	}
	public boolean isRemoteRestricted(){
		return remoteRestricted;
	}
	public void setRemoteRestricted(boolean b){
		remoteRestricted = b;
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
		actionQueue.queue(Priority.MED_PRIORITY, new ActionQueue.Action(){
			public void doAction() throws IOException{
				resendQueue.dequeueChannelMsg(localID);
				resendQueue.dequeueData(currentSeqno);
				conn.sendCloseRequest(Channel.this);
				state = CLOSE_REQ_SENT;
			}
			
		});
		
		
	}

	public int getRemoteSeqno() {
		return remoteSeqno;
	}

	public void setRemoteSeqno(int i) {
		Debug.log("Set remote seqno: " + i);
		remoteSeqno = i;
	}

	public LabCommWriter getWriter() {
		return writer;
	}

	public boolean receivedAck(ack msg) throws IOException {
		boolean rightNo = msg.seqno == currentSeqno;
		if (rightNo && !importantQueue.isEmpty()) {
			sendImportant(importantQueue.poll());
		}
		return rightNo;
	}

	private void writeData(byte[] data) throws IOException {
		boolean important = ackOnData || data[0] == LabComm.SAMPLE
				|| data[0] == LabComm.TYPEDEF;
		if (important) {
			if (importantQueue.isEmpty()) {
				sendImportant(data);
			} else {
				importantQueue.offer(data);
			}
		} else {
			conn.sendDataSample(data, important, this);
		}

	}

	private void sendImportant(byte[] toSend) throws IOException {
		if (!isClosed()) {
			currentSeqno = conn.sendDataSample(toSend, true, this);
		}
	}

	public class ChannelWriter implements LabCommWriter {

		@Override
		public void write(byte[] data) throws IOException {
			actionQueue.queue(Priority.MED_PRIORITY, new ActionQueue.Action() {
				private byte[] data;

				private ActionQueue.Action init(byte[] data) {
					this.data = data;
					return this;
				}

				public void doAction() throws IOException {
					writeData(data);
				}

			}.init(data));

		}
	}
}
