﻿#summary About Project.
#labels Phase-Requirements,Phase-Design
#sidebar TableOfContents




# Introduction #
Packet capturing in high-speed networks is not an easy task due to the system limitations like RAM and system bus throughput. Unfortunately, at high-rate speeds, it is often impossible to capture every packet arriving at a network interface. Hardware may not be the only bottleneck, however, as inefficient software is also to blame for poor resources usage and resulting packet loss during capture. Within the context of this project the new software components will be developed in order to improve the performance of packet capturing in [FreeBSD](http://www.freebsd.org).



# Project Goal #
Implementation the new software components for efficient packet capturing at 1Gb and 10Gb in _FreeBSD_. The new software should make it possible to minimize packet loss and CPU load during packet capture. These software components have to be realized as the new **ringmap** FreeBSD packet capturing stack and have to be based on:
  * generic network drivers:
    * currently supported drivers:
      * [em](http://www.freebsd.org/cgi/man.cgi?query=em&manpath=FreeBSD+7.0-RELEASE) for 1GbE and [ixgbe](http://www.freebsd.org/cgi/man.cgi?query=ixgbe&manpath=FreeBSD+8.0-RELEASE) for 10GbE capturing
  * _libpcap_
    * the new functions have to be developed for _libpcap_ to access packets by using _ringmap_

Also the system calls have to be implemented in order to control the sniffing process. The new implemented software must be transparent for user-space applications.
Namely, each application that uses _libpcap_ for packet capturing shouldn't require modification in order to run with the new _ringmap_ network driver and new modified _libpcap_.

# Approach #
Our goal, as mentioned above, is to enhance the packet capturing performance in FreeBSD  Operating System. To this aim, some issues from standard packet capturing software that can cause packet loss and high CPU load should be eliminated.

There are operations that are most expensive in terms of CPU cycles. Among others, these are:
  * System calls
  * Copy operations
  * Memory allocations

All above listed functions are used in the standard packet capturing software in FreeBSD. The memory buffers are allocated by the network driver for each packet received at the network interface. Then packets are filtered by BPF and then one or more times copied within the RAM. The user-space process gets access to the received packets using system call, what results in a context switch and in an additional copy operation. These operations can cause high system load and packet loss while capturing at bit-rates close to 1Gb/sec or greater ([see results chapter in the ringmap presentation](http://ringmap.googlecode.com/files/ringmap_slides.pdf)).

The main idea for solving this problem is eliminating packet copy operations by using shared memory and eliminating memory allocations by using ring buffers. Our solution is to map DMA packet-buffers into the space of user process. This allows user space process to access the captured packets directly in its own address space without any additional overhead. Thus the user-space process has the access to all packets received from the network immediately. Ringmap is also able to filter the packets by using _BPF_. The packet filtering can be accomplished by both kernel- and _libpcap-BPF_. If _BPF_ is not compiled into the kernel the _libpcap-BPF_ is used automatically.



# Supported Hardware #
  * Gigabit Ethernet adapters based on the following Intel controller chips:
    * _8254x, 8257x, 8259x_



# Work in Progress #
  * Support for hardware timestamping
  * Writing packets to the disc from within the kernel
  * 10-GbE-Packet-Capturing:
    * Multiqueue support
    * Extending _ringmap_ to support hardware packet filtering
  * Extending _ringmap_ for packet transmission


# Details and Results #
[See presentation](http://ringmap.googlecode.com/files/ringmap_slides.pdf)

# External Links #
  * [ringmap wiki pages](http://wiki.freebsd.org/AlexandreFiveg) on http://wiki.freebsd.org/
  * [Zero-Copy BPF Buffers](http://www.seccuris.com/documents/whitepapers/20070517-devsummit-zerocopybpf.pdf)
  * [Intel® Server Adapters. Introduction to 10 Gigabit Ethernet](http://www.intel.com/support/network/adapter/pro100/sb/CS-029872.htm)