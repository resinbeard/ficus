#Ficus
Realtime audio sampler api for linux devs

This development library provides your application with both multichannel playback/capture of mono wav audio files and all the flexibility and routing possibilities of a JACK client.

Ficus is distributed as a group of source files, settings like available number of input/output channels and storage banks are available as compile-time options through the configuration file, config.h

## Dependencies
 - [libsndfile](http://www.mega-nerd.com/libsndfile/)
 - [JACK](http://jackaudio.org/)

## Installing
Copy the libficus files to your target build directory and build your application like any other included '.c' source file.
```
$ cp -r ficus/* yourprogram/.
```

## Building
```
$ gcc -o yourprogram yourprogram.c libficus.c -lsndfile -ljack
```

## Examples
See README and source files in 'examples/' directory
