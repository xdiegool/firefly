using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using se.lth.control.labcomm;
using se.lth.cs.firefly;

namespace simpleexampleudp
{
	public class UDPServer : data.Handler
	{
		private UDPManager llp;
		private Channel chan;
		private Connection conn;

		public UDPServer(IPAddress addr, int port)
		{
			this.llp = new UDPManager(new IPEndPoint(addr, port));
		}

		private void RunDecode()
		{
			try {
				conn.Decoder.run();
			} catch (Exception e) {
				//Console.Out.WriteLine("Server: Decode error, " + e.Message);
			}
		}

		public void Run()
		{
			Console.Out.WriteLine("Server: Hello!");
			new Thread(this.llp.Read).Start();
			this.llp.Accept += (IPEndPoint ep) => {
					this.conn = new Connection(
							this.llp.getOutputStream(ep),
							this.llp.getInputStream(ep));
					conn.Accept += (Channel chan) => {
								chan.Opened += (Channel c) => {
									Console.Out.WriteLine("Server: " +
											"Channel open!");
									data.register(c.Decoder, this);
									data.register(c.Encoder);
									this.chan = c;
								};
								chan.Closed += (Channel c) => {
									Console.Out.WriteLine("Server: " +
											"Channel closed!");
									this.chan = null;
									this.llp.Close();
								};
								chan.RestrictionAccept += (Channel c) => {
									Console.Out.WriteLine("Server: " +
											"Restriction " +
											"requested and accepted!");
									return true;
								};
								return true;
							};
					conn.Open();
					Console.Out.WriteLine("Server: Connection opened");
					new Thread(this.RunDecode).Start();
			};
		}

		public void handle(int val)
		{
			Console.Out.WriteLine("Server: Got value: " + val);
			data.encode(chan.Encoder, 2222);
		}
	}

	public class UDPClient : data.Handler
	{
		private Channel chan;
		private Connection conn;
		private IPEndPoint local;
		private IPEndPoint remote;
		private int count;

		public UDPClient(IPEndPoint local, IPEndPoint remote)
		{
			this.count = 0;
			this.local = local;
			this.remote = remote;
		}

		private void RunDecode()
		{
			try {
				conn.Decoder.run();
			} catch (Exception e) {}
		}

		public void Run()
		{
			Console.Out.WriteLine("Client: Hello!");
			UDPManager llp = new UDPManager(this.local);
			new Thread(llp.Read).Start();
			conn = new Connection(llp.getOutputStream(this.remote),
					llp.getInputStream(this.remote));
			conn.Open();
			chan = new Channel(conn);
			chan.Opened += (Channel c) => {
				Console.Out.WriteLine("Client: Channel opened!");
				this.chan = c;
				data.register(c.Decoder, this);
				data.register(c.Encoder);
				c.RequestRestriction(true);
			};
			chan.Closed += (Channel c) => {
				Console.Out.WriteLine("Client: Channel closed!");
				this.chan = null;
				llp.Close();
			};
			chan.RestrictionUpdate += (Channel c) => {
				if (c.IsRestricted) {
					data.encode(c.Encoder, 1111);
					c.RequestRestriction(false);
				} else {
					Console.Out.WriteLine("Client: Unrestricted");
					this.conn.Close();
				}
			};
			chan.Open();
			new Thread(this.RunDecode).Start();
			Console.Out.WriteLine("Client: Done!");
		}

		public void handle(int val)
		{
			Console.Out.WriteLine("Client: Got value: " + val);
			//if (this.count == 10)
				//this.conn.Close();
			//else
				//data.encode(this.chan.Encoder, ++this.count);
		}
	}

	public class SimpleExampleUDP
	{
		static void Main(string[] args)
		{
			UDPServer srv = new UDPServer(IPAddress.Parse("127.0.0.1"), 55556);
			new Thread(srv.Run).Start();
			//UDPClient cl = new UDPClient(
					//new IPEndPoint(IPAddress.Parse("127.0.0.1"), 55555),
					//new IPEndPoint(IPAddress.Parse("127.0.0.1"), 55556));
			//new Thread(cl.Run).Start();
		}

	}
}

