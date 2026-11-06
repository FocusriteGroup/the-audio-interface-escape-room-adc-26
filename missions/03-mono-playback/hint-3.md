# Check the phaseInverted bitset

`AudioProcessor::prepareState` takes the phase button states from the UI, and processes them into values the audio processor can use.
The processed phase values are stored in a bitset: are they being written correctly?