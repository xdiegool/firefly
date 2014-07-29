package se.lth.cs.firefly;

import genproto.*;
import se.lth.control.labcomm.*;
import java.util.*;
import java.io.*;


public class ResendQueue{
	private LinkedHashMap<Integer, LabCommSample> queue;
	private Connection conn;
	private Object lock;
	private ConnectionEncoder encoder;
	private Thread res;

	public ResendQueue(ConnectionEncoder encoder, Connection conn){	
		queue = new LinkedHashMap<Integer, LabCommSample>();
		this.conn = conn;
		this.encoder = encoder;
		lock = new Object();
		res = new Thread(new Resender());
		res.start();
	}
	public void queue(int index, LabCommSample s){
Debug.log("Queueing: " +  s.getClass().getSimpleName() + " @" + index);
		synchronized(lock){
			queue.put(index, s);
			lock.notifyAll();
		 }
	}
	public void dequeue(int index){
		synchronized(lock){
			LabCommSample s = queue.remove(index);
			String printString = (s == null) ? "Already dequeued" : s.getClass().getSimpleName();
			Debug.log("Dequeueing: " +  printString  + " @" + index);
			lock.notifyAll();
		}
	}
	public void stop(){
		res.interrupt();
	}
	private class Resender implements Runnable {
	private static final int sleepTime = 300;
		public void run() {
		Debug.log("ResendThread starting");
			while (!Thread.currentThread().isInterrupted()) {
				try {
					synchronized(lock){
						for(LabCommSample s : queue.values()){
							resendSample(s);
						}
						long t0 = System.currentTimeMillis();
						long t = t0;
						while(sleepTime - (t-t0) > 0 || queue.isEmpty() && !Thread.currentThread().isInterrupted()){
							lock.wait(sleepTime- (t-t0));	
							t = queue.isEmpty() ? t0 : System.currentTimeMillis();	
						}						
					}
				
				} catch (InterruptedException e) {
					Thread.currentThread().interrupt();
				} catch (Exception e) {
					conn.exception(e);
				}
			}
		Debug.log("ResendThread stopping");
		}
		private void resendSample(LabCommSample s) throws IOException{
			// Ugly but working . TODO Fix this
			Debug.log("Resending " + s.getClass().getSimpleName());
			switch (s.getClass().getSimpleName()){
				case "channel_request":
					channel_request.encode(encoder,(channel_request) s);
					break;
				case "channel_response":
					channel_response.encode(encoder,(channel_response) s);
					break;
				case "channel_ack":
					channel_ack.encode(encoder,(channel_ack) s);
					break;
				case "channel_close":
					channel_close.encode(encoder,(channel_close) s);
					break;
				case "channel_restrict_ack":
					channel_restrict_ack.encode(encoder,(channel_restrict_ack) s);
					break;
				case "channel_restrict_request":
					channel_restrict_request.encode(encoder,(channel_restrict_request) s);
					break;
				case "ack":
					ack.encode(encoder,(ack) s);
					break;
				case "data_sample":
					data_sample.encode(encoder,(data_sample) s);
					break;
				default: 
					Debug.log("No type matched in resend sample");		
			}
		}
	}
}
