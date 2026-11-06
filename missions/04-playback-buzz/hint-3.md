# How many samples are copied each time the buffer is processed?

Take a look at `copyOutputBlock` in `OutputBuffer`. Here, audio samples are being copied from the source to the destination buffer.
Each time the buffer is copied, we seem to be losing a sample. What could be causing that?