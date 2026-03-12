using ManagedBass.Asio;
using NUnit.Framework;
using System;
using System.IO;
using System.Threading;

namespace ManagedBass.Sox.Test
{
    [Explicit]
    [TestFixture(BassFlags.Default, @"toby fox - UNDERTALE Soundtrack - 59 Spider Dance.mp3")]
    [TestFixture(BassFlags.Default, @"toby fox - UNDERTALE Soundtrack - 999 MEGALOVANIA.mp3")]
    [TestFixture(BassFlags.Default | BassFlags.Float, @"toby fox - UNDERTALE Soundtrack - 59 Spider Dance.mp3")]
    [TestFixture(BassFlags.Default | BassFlags.Float, @"toby fox - UNDERTALE Soundtrack - 999 MEGALOVANIA.mp3")]
    public class E2ETests
    {
        const int OUTPUT_RATE = 192000;

        private static readonly string Location = Path.GetDirectoryName(typeof(Tests).Assembly.Location);

        public E2ETests(BassFlags bassFlags, string fileName)
        {
            this.BassFlags = bassFlags;
            this.FileName = Path.Combine(Location, "Media", fileName);
        }

        public BassFlags BassFlags { get; private set; }

        public string FileName { get; private set; }

        [SetUp]
        public void SetUp()
        {
            Assert.IsTrue(Loader.Load("bass"));
            Assert.IsTrue(Loader.Load("bassasio"));
            Assert.IsTrue(BassSox.Load());
            Assert.IsTrue(BassSox.Asio.Load());
        }

        [TearDown]
        public void TearDown()
        {
            BassSox.Unload();
            BassSox.Asio.Unload();
            Bass.Free();
        }

        /// <summary>
        /// A basic end to end test.
        /// </summary>
        [Test]
        public void DirectSound_E2E()
        {
            Assert.IsTrue(Bass.Init(Bass.DefaultDevice, OUTPUT_RATE));

            var sourceChannel = Bass.CreateStream(this.FileName, Flags: this.BassFlags | BassFlags.Decode);
            if (sourceChannel == 0)
            {
                Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var playbackChannel = BassSox.StreamCreate(OUTPUT_RATE, this.BassFlags, sourceChannel);
            if (playbackChannel == 0)
            {
                Assert.Fail(string.Format("Failed to create playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            this.Configure(playbackChannel, false);

            if (!BassSox.StreamBuffer(playbackChannel))
            {
                Assert.Fail(string.Format("Failed to buffer the playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            if (!Bass.ChannelPlay(playbackChannel))
            {
                Assert.Fail(string.Format("Failed to play the playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var channelLength = Bass.ChannelGetLength(sourceChannel);
            var channelLengthSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelLength);

            do
            {
                if (Bass.ChannelIsActive(playbackChannel) == PlaybackState.Stopped)
                {
                    break;
                }

                var channelPosition = Bass.ChannelGetPosition(sourceChannel);
                var channelPositionSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelPosition);

                TestContext.Out.WriteLine(
                    "{0}/{1}",
                    TimeSpan.FromSeconds(channelPositionSeconds).ToString("g"),
                    TimeSpan.FromSeconds(channelLengthSeconds).ToString("g")
                );

                var bufferLength = default(int);
                if (BassSox.StreamBufferLength(playbackChannel, out bufferLength))
                {
                    TestContext.Out.WriteLine(
                        "Buffer: {0}ms",
                        bufferLength
                    );
                }

                Thread.Sleep(1000);
            } while (true);

            if (!Bass.StreamFree(playbackChannel))
            {
                Assert.Fail(string.Format("Failed to free the playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            if (!Bass.StreamFree(sourceChannel))
            {
                Assert.Fail(string.Format("Failed to free the source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }
        }

        /// <summary>
        /// A basic end to end test.
        /// </summary>
        [Test]
        [Explicit]
        public void ASIO_E2E()
        {
            Assert.IsTrue(Bass.Init(Bass.NoSoundDevice));

            var sourceChannel = Bass.CreateStream(this.FileName, Flags: this.BassFlags | BassFlags.Decode);
            if (sourceChannel == 0)
            {
                Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var playbackChannel = BassSox.StreamCreate(OUTPUT_RATE, this.BassFlags | BassFlags.Decode, sourceChannel);
            if (playbackChannel == 0)
            {
                Assert.Fail(string.Format("Failed to create playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            this.Configure(playbackChannel, true);

            if (!BassAsio.Init(0, AsioInitFlags.Thread))
            {
                Assert.Fail(string.Format("Failed to initialize ASIO: {0}", Enum.GetName(typeof(Errors), BassAsio.LastError)));
            }

            if (!BassSox.Asio.StreamSet(playbackChannel))
            {
                Assert.Fail("Failed to set ASIO stream.");
            }

            if (!BassSox.Asio.ChannelEnable(false, 0))
            {
                Assert.Fail(string.Format("Failed to enable ASIO: {0}", Enum.GetName(typeof(Errors), BassAsio.LastError)));
            }

            if (!BassAsio.ChannelJoin(false, 1, 0))
            {
                Assert.Fail(string.Format("Failed to enable ASIO: {0}", Enum.GetName(typeof(Errors), BassAsio.LastError)));
            }

            if (!BassAsio.ChannelSetRate(false, 0, OUTPUT_RATE))
            {
                Assert.Fail(string.Format("Failed to set ASIO rate: {0}", Enum.GetName(typeof(Errors), BassAsio.LastError)));
            }

            if (this.BassFlags.HasFlag(BassFlags.Float))
            {
                if (!BassAsio.ChannelSetFormat(false, 0, AsioSampleFormat.Float))
                {
                    Assert.Fail(string.Format("Failed to set ASIO format: {0}", Enum.GetName(typeof(Errors), BassAsio.LastError)));
                }
            }
            else
            {
                if (!BassAsio.ChannelSetFormat(false, 0, AsioSampleFormat.Bit16))
                {
                    Assert.Fail(string.Format("Failed to set ASIO format: {0}", Enum.GetName(typeof(Errors), BassAsio.LastError)));
                }
            }

            BassAsio.Rate = OUTPUT_RATE;

            if (!BassAsio.Start())
            {
                Assert.Fail(string.Format("Failed to start ASIO: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var channelLength = Bass.ChannelGetLength(sourceChannel);
            var channelLengthSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelLength);

            do
            {
                if (Bass.ChannelIsActive(playbackChannel) == PlaybackState.Stopped)
                {
                    break;
                }

                var channelPosition = Bass.ChannelGetPosition(sourceChannel);
                var channelPositionSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelPosition);

                TestContext.Out.WriteLine(
                    "Position: {0}/{1}",
                    TimeSpan.FromSeconds(channelPositionSeconds).ToString("g"),
                    TimeSpan.FromSeconds(channelLengthSeconds).ToString("g")
                );

                var bufferLength = default(int);
                if (BassSox.StreamBufferLength(playbackChannel, out bufferLength))
                {
                    TestContext.Out.WriteLine(
                        "Buffer: {0}ms",
                        bufferLength
                    );
                }

                Thread.Sleep(1000);
            } while (true);

            BassAsio.Stop();

            Bass.StreamFree(playbackChannel);
            Bass.StreamFree(sourceChannel);
        }

        public void Configure(int channelHandle, bool background)
        {
            {
                var ok = true;
                ok &= BassSox.ChannelSetAttribute(channelHandle, SoxChannelAttribute.Quality, SoxChannelQuality.Ultra);
                ok &= BassSox.ChannelSetAttribute(channelHandle, SoxChannelAttribute.Phase, SoxChannelPhase.Intermediate);
                ok &= BassSox.ChannelSetAttribute(channelHandle, SoxChannelAttribute.SteepFilter, true);
                ok &= BassSox.ChannelSetAttribute(channelHandle, SoxChannelAttribute.Background, background);
                ok &= BassSox.ChannelSetAttribute(channelHandle, SoxChannelAttribute.NoDither, true);
                Assert.IsTrue(ok, "Failed to set channel attribute.");
            }

            {
                var ok = true;
                var value = default(int);
                ok &= BassSox.ChannelGetAttribute(channelHandle, SoxChannelAttribute.Quality, out value);
                Assert.AreEqual((int)SoxChannelQuality.Ultra, value);
                ok &= BassSox.ChannelGetAttribute(channelHandle, SoxChannelAttribute.Phase, out value);
                Assert.AreEqual((int)SoxChannelPhase.Intermediate, value);
                ok &= BassSox.ChannelGetAttribute(channelHandle, SoxChannelAttribute.SteepFilter, out value);
                Assert.AreEqual(1, value);
                ok &= BassSox.ChannelGetAttribute(channelHandle, SoxChannelAttribute.Background, out value);
                Assert.AreEqual(background, Convert.ToBoolean(value));
                ok &= BassSox.ChannelGetAttribute(channelHandle, SoxChannelAttribute.NoDither, out value);
                Assert.AreEqual(1, value);
                Assert.IsTrue(ok, "Failed to get channel attribute.");
            }
        }
    }
}
