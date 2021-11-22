# Enigmatic

Disk streaming, system monitor, and client programming thing.

The daemon will stream to disk (saving a history which can be
played back). The clients are at the disposal of a developer.

The daemon compresses blocks using LZ4 compression, and when
rotating will again crush/compress the historical log file.

Writing a client tool is fairly straightforward. Check the
examples folder for some example uses. The client API is
event-driven.

Currently CPU cores are logged every 1/10 second and all other
system resources by the second.

Tested, as and when possible on Linux, FreeBSD and OpenBSD on
some unusualish hardware.

## Usage

Generally speaking write a client which executes the enigmatic
daemon.

To stop the daemon:

   $ enigmatic -s

## Bugs

None :)
