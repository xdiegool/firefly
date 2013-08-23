using System;
using System.IO;
using System.Collections.Generic;
using se.lth.control.labcomm;

namespace se.lth.cs.firefly {

	public delegate void ChannelOpened(Channel chan);
	public delegate void ChannelClosed(Channel chan);
	public delegate void RestrictionChanged(Channel chan);
	public delegate bool RestrictionAcceptHandler(Channel chan);

	public class Channel {

		private enum State {Ready, Open, Closed};
		private State state;
		private int _localID;
		private int _remoteID;
		private delegate void ImportantAction();
		private List<ImportantAction> importantQueue;
		private int currentSeqNo;
		private int remoteSeqNo;
		private bool importantBusy;
		private object _restrictLock;
		private bool restrictedLocal;
		private bool restrictedRemote;
		private Connection conn;
		private LabCommDecoderChannel dec;
		private LabCommEncoderChannel enc;
		private MemoryStream decStream;
		private ChannelEncoderStream encStream;

		public Channel(Connection conn)
		{
			this._localID = -1;
			this._remoteID = -1;
			this.importantQueue = new List<ImportantAction>();
			this.currentSeqNo = 0;
			this.remoteSeqNo = 0;
			this.importantBusy = false;
			this._restrictLock = new object();
			this.restrictedLocal = false;
			this.restrictedRemote = false;
			this.conn = conn;
			this.state = State.Ready;

			this.encStream = new ChannelEncoderStream(this);
			this.decStream = new MemoryStream();
		}

		public void setRemoteID(int id) { this._remoteID = id; }
		public Connection Conn { get { return conn; } }
		public int localID {
			get { return _localID; }
			set { _localID = value; }
		}
		public int remoteID {
			get { return _remoteID; }
			set { _remoteID = value; }
		}
		public LabCommDecoderChannel Decoder { get { return this.dec; } }
		public LabCommEncoderChannel Encoder { get { return this.enc; } }

		private ChannelOpened _opened;
		public event ChannelOpened Opened
		{
			add
			{
				_opened += value;
				if (this.state == State.Open)
					value(this);
			}
			remove { _opened -= value; }
		}
		
		public void Open()
		{
			this.conn.OpenChannel(this);
		}

		public void HandshakeDone(bool successful)
		{
			enc = new ChannelEncoder(this.encStream, this);
			dec = new LabCommDecoderChannel(this.decStream, false);
			this.state = State.Open;
			if (_opened != null)
				_opened(this);
		}

		private ChannelClosed _closed;
		public event ChannelClosed Closed
		{
			add
			{
				_closed += value;
				if (this.state == State.Closed)
					value(this);
			}
			remove { _closed -= value; }
		}

		private void SetClosed()
		{
			Console.Out.WriteLine("Channel: important queue size {0}, " +
					"currentSeqNo {1}, remoteSeqNo {2}",
					this.importantQueue.Count, this.currentSeqNo,
					this.remoteSeqNo);
			this.state = State.Closed;
			if (_closed != null)
				_closed(this);
		}

		public void Close()
		{
			this.conn.CloseChannel(this);
			SetClosed();
		}

		public void WasClosed()
		{
			SetClosed();
		}

		public bool IsRestricted {
			get
			{
				lock (this._restrictLock) {
					return this.restrictedLocal && this.restrictedRemote;
				}
			}
		}

		public event RestrictionChanged RestrictionUpdate;

		public void RequestRestriction(bool restricted)
		{
			lock (this._restrictLock) {
				if (this.restrictedLocal == restricted) {
					//Console.Out.WriteLine("Channel: Ignoring restrict request");
					return;
				}
			}
			lock (this.importantQueue) {
				if (importantBusy) {
					//Console.Out.WriteLine("Chan: queueing restrict request for {0}", restricted);
					this.importantQueue.Add(
						delegate() {
							lock (this._restrictLock) {
								this.restrictedLocal = restricted;
							}
							this.importantBusy = true;
							this.Conn.RequestRestriction(this, restricted);
						}
					);
				} else {
					lock (this._restrictLock) {
						this.restrictedLocal = restricted;
					}
					this.importantBusy = true;
					this.Conn.RequestRestriction(this, restricted);
				}
			}
		}

		public void RestrictionAcked(bool acked)
		{
			lock (this._restrictLock) {
				if (restrictedLocal == acked) {
					if (restrictedLocal) {
						this.restrictedRemote = true;
						if (RestrictionUpdate != null)
							RestrictionUpdate(this);
					} else {
						this.restrictedRemote = false;
						if (RestrictionUpdate != null)
							RestrictionUpdate(this);
					}
				} else {
					if (restrictedLocal) {
						this.restrictedLocal = false;
						this.restrictedRemote = false;
						if (RestrictionUpdate != null)
							RestrictionUpdate(this);
					} else {
						// TODO error inconsistent state
					}
				}
			}
			Ack();
		}

		public event RestrictionAcceptHandler RestrictionAccept;
		public void RestrictionRequested(bool restricted)
		{
			bool ack = restricted;
			lock (this._restrictLock) {
				if (!this.restrictedLocal) {
					if (restricted) {
						ack = false;
						if (RestrictionAccept != null)
							ack = RestrictionAccept(this);
					}
				}
				bool tmp = IsRestricted;
				this.restrictedRemote = ack;
				this.restrictedLocal = ack;
				if (tmp != IsRestricted) {
					if (RestrictionUpdate != null)
						RestrictionUpdate(this);
				}
			}
			this.Conn.SendRestrictAck(this, ack);
		}

		private int NextSeqNo()
		{
			if (++this.currentSeqNo <= 0)
				this.currentSeqNo = 1;
			return this.currentSeqNo;
		}

		public void Read(byte[] data, int seqno)
		{
			int count = data.Length;
			if (this.state != State.Open) {
				// TODO Error
				return;
			}
			lock (this.importantQueue) {
				if (seqno == 0 || seqno > remoteSeqNo) {
					this.decStream.Write(data, 0, data.Length);
					this.decStream.Seek(0, SeekOrigin.Begin);
					try {
						this.dec.runOne();
					}
					catch (EndOfStreamException e) {
						//Console.Out.WriteLine("Channel: Decoder Error, " + e.Message);
					}
					this.decStream.SetLength(0);
					if (seqno != 0) {
						this.Conn.SendAck(this, seqno);
						this.remoteSeqNo = seqno;
					}
				}
			}
		}

		public void Write(byte[] data, bool important)
		{
			if (important) {
				lock (this.importantQueue) {
					if (importantBusy) {
						this.importantQueue.Add(delegate() {
							this.importantBusy = true;
							this.Conn.SendDataSample(this, data,
								this.NextSeqNo());
						});
					} else {
						this.importantBusy = true;
						this.Conn.SendDataSample(this, data,
								this.NextSeqNo());
					}
				}
			} else {
				this.Conn.SendDataSample(this, data, 0);
			}
		}

		public int Ack()
		{
			lock (this.importantQueue) {
				if (this.importantQueue.Count > 0) {
					this.importantQueue[0]();
					this.importantQueue.RemoveAt(0);
				} else {
					this.importantBusy = false;
				}
			}
			return 0;
		}

		public int Ack(int seqno)
		{
			int res = 0;
			lock (this.importantQueue) {
				if (seqno == currentSeqNo) {
					res = Ack();
				}
			}
			return res;
		}

		private class ChannelEncoderStream : MemoryStream
		{

			private Channel chan;

			public ChannelEncoderStream(Channel chan) : base()
			{
				this.chan = chan;
			}

			public override void Flush()
			{
				byte[] data = ToArray();
				this.chan.Write(data, false);
				Position = 0;
			}

		}

		private class ChannelEncoder : LabCommEncoderChannel
		{
			private Channel chan;

			public ChannelEncoder(Stream stream, Channel chan) : base(stream, false)
			{
				this.chan = chan;
			}

			public override void register(LabCommDispatcher dispatcher)
			{
				int index = registry.add(dispatcher);
				encodePacked32(LabComm.SAMPLE);
				encodePacked32(index);
				encodeString(dispatcher.getName());
				byte[] signature = dispatcher.getSignature();
				for (int i = 0 ; i < signature.Length ; i++) {
					encodeByte(signature[i]);
				}
				byte[] data = bytes.ToArray();
				bytes.SetLength(0);
				this.chan.Write(data, true);
			}
		}
	}
}
