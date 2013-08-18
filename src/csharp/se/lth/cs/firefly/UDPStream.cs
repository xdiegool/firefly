using System;
using System.Threading;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Collections.Generic;

namespace se.lth.cs.firefly {
	public class UDPOutputStream : MemoryStream
	{
		private IPEndPoint remote;
		private UdpClient client;
		public UDPOutputStream(IPEndPoint remote, UdpClient client)
		{
			this.remote = remote;
			this.client = client;
		}

		public override void Flush()
		{
			byte[] data = ToArray();
			this.client.Send(data, data.Length, this.remote);
			SetLength(0);
		}
	}

	public class UDPInputStream : MemoryStream
	{

		public override int Read(byte[] data, int offset, int length)
		{
			lock(this)
			{
				while ((Length - Position) < length)
					Monitor.Wait(this);
				Console.Out.WriteLine("INStream: Pos {0}, Len {1}, Cap {2}",
						Position, Length, Capacity);
				int res = 0;
				res = base.Read(data, offset, length);
				return res;
			}
		}

		public override void Write(byte[] data, int offset, int length)
		{
			lock(this)
			{
				if (Length >= 256) {
					// Dont save all trafic, reset stream when 256 bytes is
					// buffered
					byte[] cache = new byte[Length - Position];
					int res = 0;
					while (res < cache.Length)
						res += Read(cache, res, cache.Length - res);
					SetLength(0);
					base.Write(cache, 0, cache.Length);
				}
				long tmp = Position;
				Seek(0, SeekOrigin.End);
				base.Write(data, offset, length);
				Position = tmp;
				Monitor.PulseAll(this);
			}
		}
	}

	public delegate void ConnectionUDPAccept(IPEndPoint ep);
	public class UDPManager
	{

		private bool closed;
		private UdpClient client;
		private Dictionary<IPEndPoint, UDPInputStream> inStreams;
		private Dictionary<IPEndPoint, UDPOutputStream> outStreams;

		public UDPManager(IPEndPoint local)
		{
			this.closed = false;
			this.client = new UdpClient(local);
			inStreams = new Dictionary<IPEndPoint, UDPInputStream>();
			outStreams = new Dictionary<IPEndPoint, UDPOutputStream>();
		}

		public UDPOutputStream getOutputStream(IPEndPoint ep)
		{
			lock (this.outStreams) {
				if (this.closed)
					throw new IOException("UDPManager is closed");
				if (!outStreams.ContainsKey(ep))
					outStreams[ep] = new UDPOutputStream(ep, this.client);
				return outStreams[ep];
			}
		}

		public UDPInputStream getInputStream(IPEndPoint ep)
		{
			lock (this.inStreams) {
				if (this.closed)
					throw new IOException("UDPManager is closed");
				if (!inStreams.ContainsKey(ep))
					inStreams[ep] = new UDPInputStream();
				return inStreams[ep];
			}
		}

		public event ConnectionUDPAccept Accept;

		public void Read()
		{
			IPEndPoint from = null;
			while (true) {
				//Console.Out.WriteLine("UDPMan: Reading...");
				try {
					byte[] data = this.client.Receive(ref from);
				//Console.Out.WriteLine("UDPMan: Done.");
					lock (this.inStreams) {
						if (this.inStreams.ContainsKey(from)) {
							this.inStreams[from].Write(data, 0, data.Length);
						} else {
							if (Accept != null)
								Accept(from);
							if (this.inStreams.ContainsKey(from))
								this.inStreams[from].Write(data, 0, data.Length);
						}
					}
				}
				catch (SocketException e)
				{
					return;
				}
			}
		}

		public void Close()
		{
			lock (inStreams) { lock (outStreams) {
				this.closed = true;
				this.client.Close();
				foreach (UDPInputStream s in this.inStreams.Values) {
					s.Close();
				}
				foreach (UDPOutputStream s in this.outStreams.Values) {
					s.Close();
				}
			} }

		}
	}
}
