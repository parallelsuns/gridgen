# gridgen
Simple utility to join multiple audio samples into a single file for grid-based slicing on various music production devices.

##Parameters

Minimum Grid Size: Specify a minimum number of slices for the output file. Useful for devices/software that can only chop samples to specific number of slices. Leave at 0 for no minimum.

Max Length Per Sample: Specifies the max length per audio sample. Samples longer than this value will be truncated. If specified, the final output will be (Max Length * Number of Samples) long in seconds. Leave at 0 for no maximum length.

Supports Windows only.

GridGen Copyright (C) 2024 Benedict Lee

AudioFile Copyright (c) 2017 Adam Stark

libsamplerate Copyright 2016 Erik de Castro Lopo
