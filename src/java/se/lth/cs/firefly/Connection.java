package se.lth.cs.firefly;

import se.lth.control.labcomm.*;
import genproto.*;
import java.util.*;
import java.io.*;
import java.net.*;


public abstract class Connection implements
							   ack.Handler,
							   channel_ack.Handler,
							   channel_close.Handler,
							   channel_request.Handler,
							   channel_response.Handler,
							   channel_restrict_ack.Handler,
							   channel_restrict_request.Handler,
							   data_sample.Handler{

	private HashMap<Integer, Channel> channels;
	private int nextChannelID;
	protected FireflyApplication delegate;
	private ConnectionDecoder bottomDecoder;
	private ConnectionEncoder bottomEncoder;
	private int seqno;
	private boolean ackOnData;
	protected int localPort;
	private ResendQueue resendQueue;
	protected InetAddress remoteAddress;
	protected int remotePort;
	private Thread receiverThread;

	protected Connection(int localPort, FireflyApplication delegate)  throws IOException{
		this.delegate=delegate;
		this.localPort=localPort;
		seqno = Integer.MIN_VALUE+1; //+1 bc.of. Channel latestWrittenID to work
		channels = new HashMap<Integer, Channel>();	
		nextChannelID = 0;
		ackOnData = false;
		channels = new HashMap<Integer, Channel>();	
		bottomDecoder = new ConnectionDecoder(new AppendableInputStream());
		bottomEncoder = new ConnectionEncoder(new ConnectionWriter(), bottomDecoder);
		resendQueue = new ResendQueue(bottomEncoder, this);
		
		init();
		
		data_sample.register(bottomDecoder, this);
		ack.register(bottomDecoder, this);
		channel_request.register(bottomDecoder, this);
		channel_response.register(bottomDecoder, this);
		channel_ack.register(bottomDecoder, this);
		channel_close.register(bottomDecoder, this);
		channel_restrict_request.register(bottomDecoder, this);
		channel_restrict_ack.register(bottomDecoder, this);

		receiverThread = new Thread(new Receiver());
		receiverThread.start();
		
	}
	protected Connection(int localPort, int remotePort, InetAddress remoteAddress, FireflyApplication delegate) throws IOException{
		this(localPort, delegate);
		this.remotePort=remotePort;
		this.remoteAddress = remoteAddress;
		startSending();
		
	}

	protected void startSending() throws IOException{
		data_sample.register(bottomEncoder);
		ack.register(bottomEncoder);
		channel_request.register(bottomEncoder);
		channel_response.register(bottomEncoder);
		channel_ack.register(bottomEncoder);
		channel_close.register(bottomEncoder);
		channel_restrict_request.register(bottomEncoder);
		channel_restrict_ack.register(bottomEncoder);
	}

	protected abstract void init() throws IOException;
	protected abstract int receive(byte[] b, int len) throws IOException;
	protected abstract int bufferSize()  throws IOException;
	protected abstract void send(byte[] b, int length) throws IOException; 
	protected abstract void closeTransport();
	public synchronized void exception(Exception e){
		e.printStackTrace();
		delegate.connectionError(this);
	}

	public synchronized void close(){
		closeTransport();
		receiverThread.interrupt();
		resendQueue.stop();	
		Debug.log("Close connection");	
	}
	
	/*###################### CHANNEL HANDLING ###################*/
	/**
	* Sends a request to the connected firefly node
	* asking to open a channel. 
	*/
	public final synchronized void openChannel() throws IOException{
		Channel chan = new Channel(nextChannelID, this);
		channels.put(nextChannelID, chan);
		channel_request req = new channel_request();
		req.source_chan_id = nextChannelID;
		Debug.log("Sending channel request");
		channel_request.encode(bottomEncoder, req);	
		resendQueue.queue(nextChannelID, req); //Resend until response
		nextChannelID++;
	}

	/**
	* Sends a request to the connected firefly node
	* asking to close a channel. 
	*
	* @param chan the channel to be closed 
	*/
	public final synchronized void closeChannel(Channel chan) throws IOException{
		channel_close cc = new channel_close();
		cc.source_chan_id = chan.getLocalID();
		cc.dest_chan_id = chan.getRemoteID();
		channel_close.encode(bottomEncoder, cc);
		resendQueue.queue(cc.source_chan_id, cc); //Resend until ack		
	}

	/*###################### LABCOMM CALLBACKS ###################*/
	/**
	* LabComm callback
	* 
	* A channel request has been received. Three cases:
	* 1 We have received it before but delegate declined
	* => Check again and send response
	* 2 We have received it and delegate accepted (i.e. it exists in channels)
	* => send positive response again
	* 3 We haven't received it before
	* => Check with delegate and send response
	*
	* 1 and 3 have the same outcome so we can bundle them.
	*/
	public final synchronized void handle_channel_request(channel_request req) throws Exception{
		Debug.log("Got channel request");
		channel_response resp = new channel_response();
		resp.dest_chan_id = req.source_chan_id;
		if(channels.get(req.dest_chan_id) != null){ // 2
			resp.ack = true;
		}else{ // 1 & 3 
			if (delegate.channelAccept(this)) {
				
				Channel chan = new Channel(nextChannelID, this);
				channels.put(nextChannelID, chan);
				resp.source_chan_id = nextChannelID++;
				resp.ack = true;
			} else {
				resp.ack = false;
			}
		}
		Debug.log("Sending channel response");
		channel_response.encode(bottomEncoder, resp);
		resendQueue.queue(resp.source_chan_id, resp); // Resend until ack
	}
	/**
	* LabComm callback
	* 
	* A channel response has been received from remote host on an
	* channel request from this host.
	*
	* Cases:
	* 1 We have received it before
	* => send ack
	* 2 We have not received it before
	*	 a. It was positive
	*    => OPEN the channel that we requested.
	*    b. It was negative
	*	 => remove channel from channels
	* => Send ack regardless of a or b	
	* Outcome of 1 is always done.
	* 
	* @param resp the channel_response informing whether or not
	* or request was accepted by the remote node. 
	*/
	public final synchronized void handle_channel_response(channel_response resp) throws Exception{
		Debug.log("Got channel response");
		channel_ack ack = new channel_ack();
		ack.source_chan_id = resp.dest_chan_id;
		ack.dest_chan_id = resp.source_chan_id;
		ack.ack = resp.ack;
		Debug.log("Sending channel ack");
		channel_ack.encode(bottomEncoder, ack);
		Channel chan = channels.get(resp.dest_chan_id);
		if(chan != null && !chan.isOpen()){ // 2
			if (resp.ack) { //2a
				chan.setRemoteID(resp.source_chan_id);
				chan.setEncoder(new ChannelEncoder(new channelToConnectionWriter(resp.source_chan_id))); 
				chan.setDecoder(new ChannelDecoder(new AppendableInputStream()));
				chan.setOpen();
				delegate.channelOpened(chan);
			}else{ //2b
				channels.remove(resp.dest_chan_id);
			}
			resendQueue.dequeue(resp.dest_chan_id); // Removes request		
		}
		
	}

	/**
	* LabComm callback
	* 
	* An ack regarding channels has been received, i.e., the remote host has
	* sent ack on a channel_response from this host. 
	*
	* Cases:
	* 1 We have not received this ack before 
	* => If ack is true, we OPEN the connection, else, close it and
	* inform delegate.
 	* 2 We jave received this ack before (i.e. channel is either already open (prev_ack == true) or does not exist (prev_ack == false) 
	* => disregard
	*/
	public final synchronized void handle_channel_ack(channel_ack value) throws Exception{
		Debug.log("Got channel ack");
		Channel chan = channels.get(value.dest_chan_id);
		if(chan != null && !chan.isOpen()){ // We have not received this ack before, 
			if(value.ack){ 
				chan.setEncoder(new ChannelEncoder(new channelToConnectionWriter(value.dest_chan_id))); 
				chan.setDecoder(new ChannelDecoder(new AppendableInputStream()));
				chan.setRemoteID(value.source_chan_id);
				chan.setOpen();	
				delegate.channelOpened(chan);
			}else{
				chan.setClosed();
				delegate.channelClosed(chan);
				channels.remove(value.dest_chan_id);
			}
			resendQueue.dequeue(value.dest_chan_id);
		}
	}
	/**
	* LabComm callback
	* 
	* Remote host wants to close channel or has closed channel on
	* our request, i.e. works as both response and request. 
	*
	* If it's a request, close and remove channel and send response,
	* else it's a response and we remove the channel.
	*/
	public final synchronized void handle_channel_close(channel_close req)  throws Exception{
		Channel chan = channels.get(req.dest_chan_id);
		if (chan.isOpen()) {
			Debug.log("Got channel close request");
			// Request
			delegate.channelClosed(channels.get(req.dest_chan_id));
			channel_close resp = new channel_close();
			resp.dest_chan_id = req.source_chan_id;
			resp.source_chan_id = req.dest_chan_id;
			channel_close.encode(bottomEncoder, resp);
			chan.setClosed();
			channels.remove(req.dest_chan_id);
		} else if (chan.isClosed()) {
			Debug.log("Got channel close response");
			// Response
			channels.remove(req.dest_chan_id);
			resendQueue.dequeue(req.dest_chan_id);
		}
	}
	/**
	* LabComm callback
	*/
	public final synchronized void handle_channel_restrict_ack(channel_restrict_ack 	value)  throws Exception{
		//TODO IMPLEMENT
	}
	/**
	* LabComm callback
	*/
	public final synchronized void handle_channel_restrict_request(channel_restrict_request value)  throws Exception{
		//TODO IMPLEMENT
	}

	/**
	* LabComm callback
	* 
	* We have received a data sample. This is not related to the
	* channel handling protocol so we just send it to the right channel
	* instead. 
	*/
	public final synchronized void handle_data_sample(data_sample ds) throws Exception{
		//TODO Check so that this data hasn't been received before, maybe in Channel as well
		if(ds.important){
			ack dataAck = new ack();
			dataAck.dest_chan_id = ds.src_chan_id;
			dataAck.src_chan_id = ds.dest_chan_id;
			dataAck.seqno = ds.seqno;	
			ack.encode(bottomEncoder, dataAck);
		}	
		Channel chan = channels.get(ds.dest_chan_id);
		chan.receivedData(ds.seqno, ds.app_enc_data, ds.important);
	}
	/**
	* LabComm callback
	* 
	* We have received a data ack.
	*/
	public final synchronized void handle_ack(ack value){
		Debug.log("Received data ack");
		resendQueue.dequeue(value.seqno);
	}

	/*###################### PUBLIC HELPER CLASSES ###################*/
	public class ConnectionWriter implements LabCommWriter{
		public void write(byte[] data) throws IOException{
			send(data, data.length);
		}
	}
	public class channelToConnectionWriter implements LabCommWriter{
		private int origin;
		private channelToConnectionWriter(int origin){this.origin=origin;}
		public void write(byte[] data) throws IOException{

			data_sample ds = new data_sample();
			Channel chan = channels.get(origin);
			ds.src_chan_id = chan.getLocalID();
			ds.dest_chan_id = chan.getRemoteID();
			ds.seqno = seqno;
			seqno = (seqno == Integer.MAX_VALUE) ? Integer.MIN_VALUE : seqno+1;
			ds.app_enc_data=data;
			if(ackOnData || data[0] == LabComm.SAMPLE || data[0] == LabComm.TYPEDEF){
				ds.important=true;
				resendQueue.queue(ds.seqno, ds);
			}
			data_sample.encode(bottomEncoder, ds);
		}
	}
	/*###################### PRIVATE HELPER CLASSES ###################*/
	protected class Receiver implements Runnable {
		public void run() {
			while (!Thread.currentThread().isInterrupted()) {	
				try {	
					byte[] inc = new byte[bufferSize()];
					int length = receive(inc, inc.length);
					final byte[] toDecode = new byte[length];
					System.arraycopy(inc, 0, toDecode, 0, length);
					bottomDecoder.decode(toDecode);
				}catch (EOFException e){
					//Happens when user defined types are done sending. If this happens, we only want to continue listening for packets
				} catch (InterruptedException e) {
					Thread.currentThread().interrupt();
				}catch (SocketException e) {
					Thread.currentThread().interrupt(); // Socket closed
				}catch (Exception e) {
						exception(e);
				}
			}
		}
	}	
}
