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
	private static final int INIT				= 1;
	private static final int OPEN				= 2;
	private static final int CLOSE_REQ_SENT		= 3;
	private static final int CLOSED				= 4;
	private static final int ERROR				= -1;

	private int state;
	private ChannelEncoder encoder;
	private ChannelDecoder decoder;
	private Connection conn;

	private int localID;
	private int remoteID;
	
	private PriorityQueue<Integer> writeQueue;
	private HashMap<Integer, byte[]> sampleMapping;
	private int latestWrittenID;
	private boolean first;
	

	public Channel(int localID, Connection conn) {
		state = INIT;
		this.localID = localID;
		this.conn = conn;
		writeQueue = new PriorityQueue<Integer>();
		sampleMapping = new HashMap<Integer, byte[]> (); 
		latestWrittenID = Integer.MIN_VALUE;
		first = true;
	}
	
	public void setEncoder(ChannelEncoder e){ encoder = e;}
	public void setDecoder(ChannelDecoder d){ decoder = d;}
	public LabCommEncoder getEncoder()	{ return encoder;  }
	public LabCommDecoder getDecoder()	{ return decoder;  }
	public int            getLocalID()	{ return localID;  }
	public int            getRemoteID()	{ return remoteID; }
	public void			  setRemoteID(int id) {remoteID = id;}

	//TODO Move this to ChannelDecoder or Connection to keep consistent with ConnectionDecoder
	public void receivedData(int seqno, byte[] data, boolean importantFlag) throws Exception {
		writeQueue.offer(seqno); //Put in queue
		sampleMapping.put(seqno, data);
		while(writeQueue.peek() != null && (writeQueue.peek() == latestWrittenID+1 || first)){ //While we have the subsequent data or if it is the first write, write them
			first = false;
			int number = writeQueue.poll();
			byte[] toWrite = sampleMapping.remove(number);
			write(number, toWrite);
		}		
	}
	private void write(int seqno, byte[] data) throws Exception{
		latestWrittenID=seqno;
		decoder.decodeData(data);
	}

	
	public boolean isOpen() { return state == OPEN; }
	public boolean isClosed() { return state == CLOSED; }

	public void setOpen()   { this.state = OPEN;   }
	public void setClosed() { this.state = CLOSED; }
	public void setError()  { this.state = ERROR;  }

	public void close() throws IOException {
		this.conn.closeChannel(this);
		this.state = CLOSE_REQ_SENT;
	}

}
