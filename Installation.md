

# Download #
```
svn checkout http://ringmap.googlecode.com/svn/trunk/ ringmap-read-only
```
Or get the tarball from the [download list](http://code.google.com/p/ringmap/downloads/list)

# Installing ringmap #
## Before you start ##
Before you start to install ringmap, familiarize yourself with:
  * [Updating and Upgrading FreeBSD](http://www.freebsd.org/doc/en_US.ISO8859-1/books/handbook/updating-upgrading.html)
  * [Building and Installing a Custom Kernel](http://www.freebsd.org/doc/en_US.ISO8859-1/books/handbook/kernelconfig.html)

Following steps are required before you install ringmap:
  * Be sure you have in /usr/src the base system and the kernel source.

  * [Recompile the kernel](http://www.freebsd.org/doc/en_US.ISO8859-1/books/handbook/kernelconfig-building.html) without "em" driver:
    * remove from the kernel config file the line "device em"
    * compile and install the kernel

  * Get the ringmap source code from SVN or [download list](http://code.google.com/p/ringmap/downloads/list)

  * Put folowing lines into the /etc/make.conf:
    * EM\_RINGMAP=yes
    * LIBPCAP\_RINGMAP=yes

  * Change to the scripts/ directory. There you'll find scripts for building and installing the ringmap. Read the README in that directory for further information.

## Compiling ##
```
% cd FreeBSD_8/scripts/
% chmod 755 build_ringmap.sh
% ./build_ringmap.sh
```

## Installing ##
To do this you have to login as root or [add your user account to the sudoers](http://linux-bsd-sharing.blogspot.com/2009/03/howto-using-sudo-on-freebsd.html).
```
% chmod 755 set_ringmap.sh
% sudo ./set_ringmap.sh
```

## Using ##
To use _ringmap_ set the interface into the monitor mode:
```
sudo ifconfig em0 monitor up
```
Afterwards start an application that based on libpcap, e.g. _tcpdump_, _Wireshark_, etc...

# By questionts #
Mail me  afiveg(at)freebsd.org