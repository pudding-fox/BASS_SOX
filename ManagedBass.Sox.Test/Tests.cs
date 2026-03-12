using NUnit.Framework;
using System;
using System.IO;
using System.Threading;

namespace ManagedBass.Sox.Test
{
    [TestFixture(BassFlags.Default)]
    [TestFixture(BassFlags.Default | BassFlags.Float)]
    public class Tests
    {
        const int OUTPUT_RATE = 192000;

        const int BASS_STREAMPROC_ERROR = -1;

        const int BASS_STREAMPROC_EMPTY = 0;

        const int BASS_STREAMPROC_END = -2147483648; //0x80000000;

        static readonly string Location = Path.GetDirectoryName(typeof(Tests).Assembly.Location);

        public Tests(BassFlags bassFlags)
        {
            this.BassFlags = bassFlags;
        }

        public BassFlags BassFlags { get; private set; }

        [SetUp]
        public void SetUp()
        {
            Assert.IsTrue(Loader.Load("bass"));
            Assert.IsTrue(BassSox.Load());
            Assert.IsTrue(Bass.Init(Bass.DefaultDevice, OUTPUT_RATE));
        }

        [TearDown]
        public void TearDown()
        {
            BassSox.Unload();
            Bass.Free();
        }

        /// <summary>
        /// MAX_RESAMPLERS (10) resampler channels are supported.
        /// </summary>
        [Test]
        public void CanCreateMaxResamplers()
        {
            var channels = new int[10];
            for (var a = 0; a < 10; a++)
            {
                channels[a] = Bass.CreateStream(44100, 2, this.BassFlags | BassFlags.Decode, StreamProcedureType.Dummy);
                Assert.AreNotEqual(0, channels[a]);
            }

            var resamplers = new int[10];
            for (var a = 0; a < 10; a++)
            {
                resamplers[a] = BassSox.StreamCreate(48000, this.BassFlags | BassFlags.Decode, channels[a]);
                Assert.AreNotEqual(0, resamplers[a]);
            }

            for (var a = 0; a < 10; a++)
            {
                var success = Bass.StreamFree(resamplers[a]);
                Assert.IsTrue(success);
            }

            for (var a = 0; a < 10; a++)
            {
                Bass.StreamFree(channels[a]);
            }
        }

        /// <summary>
        /// Invalid source channel is handled.
        /// </summary>
        [Test]
        public void InvalidSourceChannel()
        {
            var channel = BassSox.StreamCreate(44100, this.BassFlags | BassFlags.Decode, 0);
            Assert.AreEqual(0, channel);

            var success = Bass.StreamFree(0);
            Assert.IsFalse(success);
        }

        /// <summary>
        /// Resampling with same input/output rate is ignored.
        /// </summary>
        [Test]
        public void Noop()
        {
            var channel = Bass.CreateStream(44100, 2, this.BassFlags | BassFlags.Decode, StreamProcedureType.Dummy);
            var resampler = BassSox.StreamCreate(44100, this.BassFlags | BassFlags.Decode, channel);
            Assert.AreEqual(channel, resampler);

            Bass.StreamFree(channel);
        }

        /// <summary>
        /// Failure to read from source does not mark the stream as "complete" when <see cref="SoxChannelAttribute.KeepAlive"/> is specified.
        /// </summary>
        /// <param name="err"></param>
        [TestCase(0, BASS_STREAMPROC_EMPTY)]
        [TestCase(0, BASS_STREAMPROC_ERROR)]
        [TestCase(0, BASS_STREAMPROC_END)]
        [TestCase(512, 512 | BASS_STREAMPROC_END)]
        public void KeepAlive(int count, int? err)
        {
            var channel = Bass.CreateStream(48000, 2, this.BassFlags | BassFlags.Decode, (handle, buffer, length, user) =>
            {
                if (err.HasValue)
                {
                    return err.Value;
                }
                return count;
            });

            var resampler = BassSox.StreamCreate(44100, this.BassFlags | BassFlags.Decode, channel);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.KeepAlive, true);

            {
                var buffer = new byte[1024];
                var length = Bass.ChannelGetData(resampler, buffer, buffer.Length);
                Assert.AreEqual(count, length);
            }

            Assert.AreEqual(PlaybackState.Playing, Bass.ChannelIsActive(resampler));

            err = null;
            Bass.ChannelSetPosition(channel, 0);

            {
                var buffer = new byte[1024];
                var length = Bass.ChannelGetData(resampler, buffer, buffer.Length);
                Assert.AreEqual(count, length);
            }

            Assert.AreEqual(PlaybackState.Playing, Bass.ChannelIsActive(resampler));

            Bass.StreamFree(channel);
            Bass.StreamFree(resampler);
        }

        /// <summary>
        /// The buffer is populated over time.
        /// </summary>
        [Test]
        public void BufferLength()
        {
            var channel = Bass.CreateStream(48000, 2, this.BassFlags | BassFlags.Decode, (handle, buffer, length, user) => length);

            var resampler = BassSox.StreamCreate(44100, this.BassFlags | BassFlags.Decode, channel);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.InputBufferLength, 1000);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.PlaybackBufferLength, 5000);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.Background, false);
            var count = 0;
            for (var a = 0; a < 100; a++)
            {
                var buffer = new byte[1024];
                count += Bass.ChannelGetData(resampler, buffer, buffer.Length);
            }

            var position = Bass.ChannelGetPosition(channel);
            var ratio = (double)48000 / (double)44100;
            var expected = (int)Math.Ceiling(position - (count * ratio));
            var actual = default(int);
            if (!BassSox.StreamBufferLength(resampler, out actual))
            {
                Assert.Fail("Failed to get SOX buffer length.");
            }

            //We allow a little bit of difference.
            Assert.LessOrEqual(Math.Abs(expected - actual), 10);

            Bass.StreamFree(channel);
            Bass.StreamFree(resampler);
        }

        [Test]
        public void BufferLength_Invalid()
        {
            var channel = Bass.CreateStream(48000, 2, this.BassFlags | BassFlags.Decode, (handle, buffer, length, user) => length);

            var resampler = BassSox.StreamCreate(44100, this.BassFlags | BassFlags.Decode, channel);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.InputBufferLength, 1000);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.PlaybackBufferLength, 500);

            {
                var buffer = new byte[1024];
                var length = Bass.ChannelGetData(resampler, buffer, buffer.Length);
                Assert.AreEqual(buffer.Length, length);
            }

            Bass.StreamFree(channel);
            Bass.StreamFree(resampler);
        }

        [Test]
        public void BufferLength_Empty()
        {
            var channel = Bass.CreateStream(48000, 2, this.BassFlags | BassFlags.Decode, (handle, buffer, length, user) => length);

            var resampler = BassSox.StreamCreate(44100, this.BassFlags | BassFlags.Decode, channel);

            {
                var length = default(int);
                BassSox.StreamBufferLength(resampler, out length);
                Assert.AreEqual(0, length);
            }

            Bass.StreamFree(channel);
            Bass.StreamFree(resampler);
        }

        /// <summary>
        /// Buffer can be cleared.
        /// </summary>
        [TestCase(null)]    //Should use default.
        [TestCase(0)]       //Should use default.
        [TestCase(1)]
        [TestCase(5)]
        public void StreamBufferClear(int? bufferLength)
        {
            var channel = Bass.CreateStream(48000, 2, this.BassFlags | BassFlags.Decode, (handle, buffer, length, user) => length);

            var resampler = BassSox.StreamCreate(44100, this.BassFlags | BassFlags.Decode, channel);

            if (bufferLength.HasValue)
            {
                BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.PlaybackBufferLength, bufferLength.Value);
            }

            BassSox.StreamBufferClear(resampler);

            for (var a = 0; a < (bufferLength.HasValue ? bufferLength.Value : 1); a++)
            {
                var buffer = new byte[1024];
                var length = Bass.ChannelGetData(resampler, buffer, buffer.Length);
                Assert.AreEqual(buffer.Length, length);
            }

            BassSox.StreamBufferClear(resampler);

            Bass.StreamFree(channel);
            Bass.StreamFree(resampler);
        }

        /// <summary>
        /// Multi-thread buffer manipulations.
        /// </summary>
        /// <param name="background"></param>
        [TestCase(false)]
        [TestCase(true)]
        public void ThreadSafe(bool background)
        {
            var channel = Bass.CreateStream(44100, 2, this.BassFlags | BassFlags.Decode, (handle, buffer, length, user) => length);

            var resampler = BassSox.StreamCreate(192000, this.BassFlags | BassFlags.Decode, channel);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.KeepAlive, true);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.Background, background);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.PlaybackBufferLength, 5);

            var shutdown = false;
            var thread1 = new Thread(() =>
            {
                while (!shutdown)
                {
                    var buffer = new byte[10240];
                    var length = Bass.ChannelGetData(resampler, buffer, buffer.Length);
                    Thread.Yield();
                    Thread.Sleep(10);
                    Thread.Yield();
                }
            });
            thread1.Start();

            var random = new Random(unchecked((int)DateTime.Now.Ticks));
            var thread2 = new Thread(() =>
             {
                 while (!shutdown)
                 {
                     BassSox.StreamBuffer(resampler);
                     Thread.Yield();
                     Thread.Sleep(10);
                     Thread.Yield();
                     BassSox.StreamBufferClear(resampler);
                     Thread.Yield();
                     Thread.Sleep(10);
                     Thread.Yield();
                     BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.PlaybackBufferLength, random.Next(10));
                     Thread.Yield();
                     Thread.Sleep(10);
                     Thread.Yield();
                 }
             });
            thread2.Start();

            Thread.Sleep(10000);
            shutdown = true;
            thread1.Join();
            thread2.Join();

            Bass.StreamFree(channel);
            Bass.StreamFree(resampler);
        }

        [TestCase(@"toby fox - UNDERTALE Soundtrack - 59 Spider Dance.mp3")]
        [TestCase(@"toby fox - UNDERTALE Soundtrack - 999 MEGALOVANIA.mp3")]
        public void Seeking(string fileName)
        {
            var channel = Bass.CreateStream(Path.Combine(Location, "Media", fileName), Flags: this.BassFlags | BassFlags.Decode);

            var resampler = BassSox.StreamCreate(192000, this.BassFlags | BassFlags.Decode, channel);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.Background, true);

            var pause = false;
            var rewind = false;
            var shutdown = false;
            var thread1 = new Thread(() =>
            {
                var buffer = new byte[10240];
                while (!shutdown)
                {
                    if (!pause)
                    {
                        var length = Bass.ChannelGetData(resampler, buffer, buffer.Length);
                        if (length < 0 || (length & BASS_STREAMPROC_END) == BASS_STREAMPROC_END)
                        {
                            shutdown = true;
                        }
                    }
                    Thread.Yield();
                    Thread.Sleep(1);
                    Thread.Yield();
                }
            });
            thread1.Start();

            var thread2 = new Thread(() =>
            {
                while (!shutdown)
                {
                    pause = true;
                    BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.Background, false);
                    var position = Bass.ChannelGetPosition(channel);
                    if (rewind)
                    {
                        position -= Bass.ChannelSeconds2Bytes(channel, 5);
                    }
                    else
                    {
                        position += Bass.ChannelSeconds2Bytes(channel, 10);
                    }
                    rewind = !rewind;
                    if (position < Bass.ChannelGetLength(channel))
                    {
                        Bass.ChannelSetPosition(channel, position);
                    }
                    BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.Background, true);
                    pause = false;
                    Thread.Yield();
                    Thread.Sleep(100);
                    Thread.Yield();
                }
            });
            thread2.Start();

            thread1.Join();
            thread2.Join();

            Bass.StreamFree(channel);
            Bass.StreamFree(resampler);
        }

        public static object[][] GetHashcodeCases()
        {
            if (Environment.Is64BitProcess)
            {
                return new object[][]
                {
                    new object[] { @"toby fox - UNDERTALE Soundtrack - 59 Spider Dance.mp3", false, 1127724972, 1484866295 },
                    new object[] { @"toby fox - UNDERTALE Soundtrack - 59 Spider Dance.mp3", true, 1127724972, 1484866295 },
                    new object[] { @"toby fox - UNDERTALE Soundtrack - 999 MEGALOVANIA.mp3", false, 1433652569, 893492820 },
                    new object[] { @"toby fox - UNDERTALE Soundtrack - 999 MEGALOVANIA.mp3", true, 1433652569, 893492820 }
                };
            }
            else
            {
                return new object[][]
                {
                    new object[] { @"toby fox - UNDERTALE Soundtrack - 59 Spider Dance.mp3", false, 1135775577, 1485253849 },
                    new object[] { @"toby fox - UNDERTALE Soundtrack - 59 Spider Dance.mp3", true, 1135775577, 1485253849 },
                    new object[] { @"toby fox - UNDERTALE Soundtrack - 999 MEGALOVANIA.mp3", false, 1450765170, 893548861 },
                    new object[] { @"toby fox - UNDERTALE Soundtrack - 999 MEGALOVANIA.mp3", true, 1450765170, 893548861 }
                };
            }
        }

        [TestCaseSource("GetHashcodeCases")]
        public void Hashcode(string fileName, bool background, int hashCode16, int hashCode32)
        {
            var channel = Bass.CreateStream(Path.Combine(Location, "Media", fileName), Flags: this.BassFlags | BassFlags.Decode);

            var resampler = BassSox.StreamCreate(192000, this.BassFlags | BassFlags.Decode, channel);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.Background, background);
            BassSox.ChannelSetAttribute(resampler, SoxChannelAttribute.NoDither, true);

            var hashCode = default(int);
            unchecked
            {
                var buffer = new byte[10240];
                var eof = default(bool);
                do
                {
                    var length = Bass.ChannelGetData(resampler, buffer, buffer.Length);
                    if (length == BASS_STREAMPROC_ERROR)
                    {
                        break;
                    }
                    else if ((length & BASS_STREAMPROC_END) == BASS_STREAMPROC_END)
                    {
                        length &= ~BASS_STREAMPROC_END;
                        eof = true;
                    }
                    for (var position = 0; position < length; position++)
                    {
                        hashCode += buffer[position].GetHashCode();
                    }
                } while (!eof);
            }

            Assert.AreEqual(PlaybackState.Stopped, Bass.ChannelIsActive(channel));
            Assert.AreEqual(PlaybackState.Stopped, Bass.ChannelIsActive(resampler));

            if (this.BassFlags.HasFlag(BassFlags.Float))
            {
                Assert.AreEqual(hashCode32, Math.Abs(hashCode));
            }
            else
            {
                Assert.AreEqual(hashCode16, Math.Abs(hashCode));
            }

            Bass.StreamFree(channel);
            Bass.StreamFree(resampler);
        }
    }
}
