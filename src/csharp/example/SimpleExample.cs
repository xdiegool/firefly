using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using se.lth.control.labcomm;
using se.lth.cs.firefly;

namespace simpleexample
{
	public class Server : data.Handler
	{
		private TcpListener server;
		private Channel chan;
		private Connection conn;

		public Server(IPAddress addr, int port)
		{
			this.server = new TcpListener(addr, port);
			this.server.Start();
		}

		private void RunDecode()
		{
			try {
				conn.Decoder.run();
			} catch (Exception e) {}
		}

		public void Run()
		{
			Console.Out.WriteLine("Server: Hello!");
			TcpClient client = server.AcceptTcpClient();
			conn = new Connection(client.GetStream());
			conn.Accept += (Channel chan) => {
						chan.Opened += (Channel c) => {
							Console.Out.WriteLine("Server: Channel open!");
							data.register(c.Decoder, this);
							data.register(c.Encoder);
							this.chan = c;
						};
						chan.Closed += (Channel c) => {
							Console.Out.WriteLine("Server: Channel closed!");
							this.chan = null;
							client.Close();
							this.server.Stop();
						};
						chan.RestrictionAccept += (Channel c) => {
							Console.Out.WriteLine("Server: Restriction " +
									"requested and accepted!");
							return true;
						};
						return true;
					};
			conn.Open();
			new Thread(this.RunDecode).Start();
		}

		public void handle(int val)
		{
			Console.Out.WriteLine("Server: Got value: " + val);
			data.encode(chan.Encoder, 2);
		}
	}

	public class Client : data.Handler
	{
		private string addr;
		private int port;
		private Channel chan;
		private Connection conn;

		public Client(string addr, int port)
		{
			this.addr = addr;
			this.port = port;
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
			TcpClient client = new TcpClient(this.addr, this.port);
			conn = new Connection(client.GetStream());
			conn.Open();
			chan = new Channel(conn);
			chan.Opened += (Channel c) => {
				Console.Out.WriteLine("Client: Channel opened!");
				this.chan = c;
				data.register(c.Decoder, this);
				data.register(c.Encoder);
				c.RequestRestriction(true);
				//data.encode(c.Encoder, 1);
			};
			chan.Closed += (Channel c) => {
				Console.Out.WriteLine("Client: Channel closed!");
				this.chan = null;
				client.Close();
			};
			chan.RestrictionUpdate += (Channel c) => {
				if (c.IsRestricted)
					data.encode(c.Encoder, 1);
				else {
					Console.Out.WriteLine("Client: Restriction denied!");
					c.Close();
				}
			};
			chan.Open();
			new Thread(this.RunDecode).Start();
			Console.Out.WriteLine("Client: Done!");
		}

		public void handle(int val)
		{
			Console.Out.WriteLine("Client: Got value: " + val);
			conn.Close();
		}
	}

	public class SimpleExample
	{
		static void Main(string[] args)
		{
			Server srv = new Server(IPAddress.Any, 51337);
			new Thread(srv.Run).Start();
			Client cl = new Client("127.0.0.1", 51337);
			new Thread(cl.Run).Start();
		}

	}
}

