package se.lth.cs.firefly;

import genproto.*;
import se.lth.control.labcomm.*;
import java.util.*;
import java.io.*;


public class ActionQueue{
	private Queue<Action> queue;
	private Object lock;
	private Thread thread;

	public ActionQueue(){	
		queue = new LinkedList<Action>();
		lock = new Object();
		thread = new Thread(new ActionAgent());
		thread.start();
	}
	public void queue(Action a){
		synchronized(lock){
			queue.offer(a);
			lock.notifyAll();
		 }
	}
	public void stop(){
		thread.interrupt();
	}
	private class ActionAgent implements Runnable {
		@Override
		public void run() {
			Debug.log("ActionAgent starting");
			while (!Thread.currentThread().isInterrupted()) {
				try {
					synchronized(lock){
						while(queue.peek() != null){
							Action a = queue.poll();
							a.doAction();
						}
						while(queue.peek() == null){lock.wait();}			
					}
				} catch (InterruptedException e) {
					Thread.currentThread().interrupt();
				}
			}
			Debug.log("ActionAgent stopping");
		}
	}
	public interface Action{
		public void doAction();
	}
	
}




