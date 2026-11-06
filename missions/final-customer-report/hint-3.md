# Hint 3 (Minute 19)

Trace the displayed capture level separately from the bus delivered to the
loopback capture. Does fixing the displayed level also fix the signal? Then look
at what the capture readout does on every audio block: how much work is that once
the take is a few seconds long?
After isolating these paths, inspect `exercise/ProcessorSetup.cpp`
and `exercise/RecordingMeter.cpp`.