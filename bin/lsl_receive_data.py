# Example program to show how to read a multi-channel time series from LSL.

from pylsl import StreamInlet, resolve_stream

# first resolve an EEG stream on the lab network
print("Looking for an EEG stream...")
streams = resolve_stream('type', 'EEG')

# create a new inlet to read from the stream
inlet = StreamInlet(streams[0])

while True:
    # get a new sample (you can also omit the timestamp part if you're not
    # interested in it)
    sample, timestamp = inlet.pull_sample(timeout=2.0)
    if sample is None:
        print("EEG stream ended.")
        break
    print(timestamp, sample)
