package se.lth.cs.firefly;

import se.lth.control.labcomm.*;
import java.io.*;
import java.util.*;

public class AppendableInputStream extends InputStream{
	private InputStream is;
	public AppendableInputStream(){ 
		byte[] version = new VersionBypass().getBypass();
		is = new ByteArrayInputStream(version);
	 }
	
	@Override
	public synchronized int read() throws IOException{
		int a = is.read();
		return a;	
	}
	public synchronized void append(byte[] data){
		is= new ByteArrayInputStream(data);
	}
	/*
	* Helper class to generate the version number for the decoders to work
	*/
	public class VersionBypass implements LabCommWriter{
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
