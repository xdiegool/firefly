using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Collections.Generic;
using se.lth.control.labcomm;

namespace se.lth.cs.firefly {

	public delegate bool ChannelAccept(Channel chan);

	public class Connection :
		data_sample.Handler,
		channel_request.Handler,
		channel_response.Handler,
		channel_ack.Handler,
		channel_close.Handler,
		ack.Handler,
		channel_restrict_request.Handler,
		channel_restrict_ack.Handler
	{

		private enum State {INIT, OPEN, CLOSED, ERROR};
		private State state;
		private ConnectionEncoder enc;
		private ConnectionDecoder dec;
		private List<Channel> channels;

		public event ChannelAccept Accept;

		public Connection(Stream stream)
		{
			this.state = State.INIT;
			enc = new ConnectionEncoder(stream, this);
			dec = new ConnectionDecoder(stream);
			channels = new List<Channel>();
		}

		public Connection(Stream outStream, Stream inStream)
		{
			this.state = State.INIT;
			enc = new ConnectionEncoder(outStream, this);
			dec = new ConnectionDecoder(inStream);
			channels = new List<Channel>();
		}

		public ConnectionDecoder Decoder { get { return dec; } }

		public void Open()
		{
			if (this.state != State.INIT) {
				// error
			}
			this.state = State.OPEN;
			data_sample.register(enc);
			channel_request.register(enc);
			channel_response.register(enc);
			channel_ack.register(enc);
			channel_close.register(enc);
			ack.register(enc);
			channel_restrict_request.register(enc);
			channel_restrict_ack.register(enc);

			data_sample.register(dec, this);
			channel_request.register(dec, this);
			channel_response.register(dec, this);
			channel_ack.register(dec, this);
			channel_close.register(dec, this);
			ack.register(dec, this);
			channel_restrict_request.register(dec, this);
			channel_restrict_ack.register(dec, this);
		}

		public void Close()
		{
			lock (this.channels) {
				this.channels.ForEach(delegate(Channel chan)
						{
							chan.Close();
						}
				);
			}
		}
		
		public void OpenChannel(Channel chan)
		{
			chan.localID = 1;
			lock (this.channels) {
				channels.Add(chan);
			}
			channel_request req = new channel_request() {
				dest_chan_id = -1,
				source_chan_id = chan.localID
			};
			channel_request.encode(this.enc, req);
		}

		public void handle(channel_request val)
		{
			bool accepted = false;
			Channel chan = new Channel(this);
			chan.localID = 1;
			chan.remoteID = val.source_chan_id;
			if (Accept != null)
				accepted = Accept(chan);
			if (accepted) {
				lock (this.channels) {
					channels.Add(chan);
				}
			}
			channel_response res = new channel_response() {
				dest_chan_id = chan.remoteID,
				source_chan_id = chan.localID,
				ack = accepted
			};
			channel_response.encode(this.enc, res);
		}

		public void handle(channel_response val)
		{
			lock (this.channels) {
				Channel chan = getChannel(val.dest_chan_id);
				if (val.ack) {
					chan.setRemoteID(val.source_chan_id);
					channel_ack ack  = new channel_ack() {
						dest_chan_id = chan.remoteID,
						source_chan_id = chan.localID,
						ack = true
					};
					channel_ack.encode(enc, ack);
					chan.HandshakeDone(true);
				}
			}
		}

		public void handle(channel_ack val)
		{
			lock (this.channels) {
				Channel chan = getChannel(val.dest_chan_id);
				if (val.ack) {
					chan.HandshakeDone(true);
				}
			}
		}

		public void CloseChannel(Channel chan)
		{
			channel_close close = new channel_close() {
				dest_chan_id = chan.remoteID,
				source_chan_id = chan.localID
			};
			channel_close.encode(enc, close);
			lock (this.channels) {
				this.channels.Remove(chan);
			}
		}

		public void handle(channel_close val)
		{
			lock (this.channels) {
				Channel chan = getChannel(val.dest_chan_id);
				if (chan != null) {
					chan.WasClosed();
					this.channels.Remove(chan);
				}
			}
		}

		public void SendDataSample(Channel chan, byte[] data, int seqno)
		{
			data_sample pkt = new data_sample() {
				dest_chan_id = chan.remoteID,
				src_chan_id = chan.localID,
				important = seqno != 0,
				seqno = seqno,
				app_enc_data = data
			};
			data_sample.encode(enc, pkt);
		}

		public void handle(data_sample val)
		{
			lock (this.channels) {
				Channel chan = getChannel(val.dest_chan_id);
				chan.Read(val.app_enc_data, val.seqno);
			}
		}

		public void SendAck(Channel chan, int seqno)
		{
			ack a = new ack() {
				dest_chan_id = chan.remoteID,
				src_chan_id = chan.localID,
				seqno = seqno
			};
			ack.encode(enc, a);
		}

		public void handle(ack val)
		{
			lock (this.channels) {
				Channel chan = getChannel(val.dest_chan_id);
				chan.Ack(val.seqno);
			}
		}

		public void RequestRestriction(Channel chan, bool restricted)
		{
			Console.Out.WriteLine("Conn: Sending restrict request for {0}", restricted);
			channel_restrict_request req = new channel_restrict_request()
			{
				dest_chan_id = chan.remoteID,
				source_chan_id = chan.localID,
				restricted = restricted
			};
			channel_restrict_request.encode(enc, req);
		}

		public void handle(channel_restrict_request val)
		{
			lock (this.channels) {
				Channel chan = getChannel(val.dest_chan_id);
				chan.RestrictionRequested(val.restricted);
			}
		}

		public void SendRestrictAck(Channel chan, bool restricted)
		{
			channel_restrict_ack a = new channel_restrict_ack()
			{
				dest_chan_id = chan.remoteID,
				source_chan_id = chan.localID,
				restricted = restricted
			};
			channel_restrict_ack.encode(enc, a);

		}

		public void handle(channel_restrict_ack val)
		{
			lock (this.channels) {
				Channel chan = getChannel(val.dest_chan_id);
				chan.RestrictionAcked(val.restricted);
			}
		}

		private Channel getChannel(int id)
		{
			Channel res = null;
			channels.ForEach(delegate(Channel chan)
				{
					if (chan.localID == id)
						res = chan;
				});
			return res;
		}

		public class ConnectionDecoder : LabCommDecoderChannel
		{
			public ConnectionDecoder(Stream stream) : base(stream, false) {}

			public void AcceptSample(int index, LabCommDispatcher d)
			{
				registry.add(index, d.getName(), d.getSignature());
			}
		}

		public class ConnectionEncoder : LabCommEncoderChannel
		{
			private Connection conn;

			public ConnectionEncoder(Stream stream, Connection conn) :
				base(stream, false)
			{
				this.conn = conn;
			}

			public override void register(LabCommDispatcher dispatcher) {
				int index = registry.add(dispatcher);
				this.conn.Decoder.AcceptSample(index, dispatcher);
			}
		}
	}
}
