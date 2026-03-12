using ManagedBass.Asio;
using NUnit.Framework;
using System;
using System.IO;
using System.Threading;

namespace ManagedBass.Sox.Test
{
    [TestFixture(BassFlags.Default)]
    [TestFixture(BassFlags.Default | BassFlags.Float)]
    public class AsioTests
    {
        const int OUTPUT_RATE = 96000;

        public AsioTests(BassFlags bassFlags)
        {
            this.BassFlags = bassFlags;
        }

        public BassFlags BassFlags { get; private set; }

        [SetUp]
        public void SetUp()
        {
            Assert.IsTrue(Loader.Load("bass"));
            Assert.IsTrue(Loader.Load("bassasio"));
            Assert.IsTrue(BassSox.Asio.Load());
            Assert.IsTrue(Bass.Init(Bass.DefaultDevice, OUTPUT_RATE));
        }

        [TearDown]
        public void TearDown()
        {
            BassSox.Asio.Unload();
            Bass.Free();
        }

        /// <summary>
        /// Resampler stream handle can be set and retrieved.
        /// </summary>
        [Test]
        public void StreamSet()
        {
            var channel = Bass.CreateStream(44100, 2, this.BassFlags | BassFlags.Decode, StreamProcedureType.Dummy);
            var resampler = BassSox.StreamCreate(48000, this.BassFlags | BassFlags.Decode, channel);

            Assert.IsTrue(BassSox.Asio.StreamSet(resampler));

            var retrieved = default(int);
            Assert.IsTrue(BassSox.Asio.StreamGet(out retrieved));
            Assert.AreEqual(resampler, retrieved);

            Assert.IsTrue(Bass.StreamFree(resampler));
            Assert.IsTrue(Bass.StreamFree(channel));
        }

        /// <summary>
        /// Cannot set a stream which isn't a resampler.
        /// </summary>
        [Test]
        public void InvalidSourceChannel()
        {
            var channel = Bass.CreateStream(44100, 2, this.BassFlags | BassFlags.Decode, StreamProcedureType.Dummy);

            Assert.IsFalse(BassSox.Asio.StreamSet(channel));

            var retrieved = default(int);
            Assert.IsFalse(BassSox.Asio.StreamGet(out retrieved));

            Assert.IsTrue(Bass.StreamFree(channel));
        }
    }
}
