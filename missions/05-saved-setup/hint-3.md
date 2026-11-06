# The channel ids used to be one-based

Take a look at `LegacyMapping.cpp`. This is where legacy preset files are updated to work with the current version of the app.
In the old preset schema, the `outputChannelMappings` were one-based. Is this being respected in the update logic?