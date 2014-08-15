package se.lth.cs.firefly.util;

import se.lth.control.labcomm.*;

import java.io.*;
import java.util.*;

/**
 * Provides a stream for the difference labcomm decoders to read
 * Provides LabComm version check bypass with a helper class. 
 * No locking on emprty stream, insteead
 */
public class AppendableInputStream extends InputStream{
	protected ByteArrayInputStream is;
	protected byte[] current;
	protected int bytesLeft;
	
	/**
	 * Constructor that initializes the stream with data as content.
	 * NO version control bypass.
	 */
	public AppendableInputStream(byte[] data){ 
		is= new ByteArrayInputStream(data);
		bytesLeft = data.length;
		current = data;
	}
	
	/**
	 * Constructor that bypasses LabComm version control
	 */
	public AppendableInputStream() {
		this(new VersionBypass().getBypass());
	}

	@Override
	public synchronized int read() throws IOException{
		int a =  is.read();
		bytesLeft = a >= 0 ? bytesLeft-1 : bytesLeft;
		
		return a;	
	}
	public synchronized void append(byte[] data) throws IOException{
		byte[] newArray = new byte[bytesLeft + data.length];
		System.arraycopy(current, current.length - bytesLeft, newArray, 0, bytesLeft);
		System.arraycopy(data, 0, newArray, bytesLeft, data.length);
		current = newArray;
		is= new ByteArrayInputStream(newArray);
		bytesLeft = newArray.length;
	}
	/*
	* Helper class to generate the version number for the decoders to work
	*/
	public static class VersionBypass implements LabCommWriter{
	byte[] a;
		public VersionBypass(){
			try{
				LabCommEncoderChannel enc = new LabCommEncoderChannel(this, true);
				enc.end(null); // To force write				
			}catch (IOException e){
				Debug.errx("Could not create version bypass");
			}
		}
		@Override
		public void write(byte[] a) throws IOException{
			this.a=a;
		}
		public byte[] getBypass(){
			return a;
		}
	}
}
