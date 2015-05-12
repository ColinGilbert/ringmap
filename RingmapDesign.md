

# Idea #
Our goal is to enhance the packet capturing performance in **FreeBSD** Operating System.
To this aim, some issues from standard packet capturing software that can
cause packet loss and high CPU load should be eliminated.

There are operations that are most expensive in terms of CPU cycles. Among others,
these are:
  * System calls
  * Copy operations
  * Memory allocations

All above listed functions  are used in the standard packet capturing software
in FreeBSD. The memory buffers are allocated by the network driver for each
packet received at the network interface. Also packets are filtered and one
or more times copied within the RAM during capturing. These operations can
cause high system load and packet loss while capturing at bit-rates
closely to _1Gb/sec_ or greater  (see results chapter in the [ringmap presentation](http://ringmap.googlecode.com/files/ringmap_slides.pdf)).

The main idea of our solution to this problem is eliminating packet copy
operations by using shared memory buffers and eliminating memory allocations by
using ring buffers. The system calls that are used for packets access in FreeBSD-7
can be eliminated by using shared memory too (see [Zero-Copy BPF Buffers presentation](http://www.seccuris.com/documents/whitepapers/20070517-devsummit-zerocopybpf.pdf)).

Our solution is to map DMA packet-buffers into the space of user process. This
allows user space process to access the captured packets directly in
its own address space without any additional overhead. So, the user space
process has the access to all packets received from the network. To filter the
received packets in the user space the BPF of _libpcap_ can be used.

# Background #
This chapter explains the software and hardware aspects relevant for packet capturing.

## Packet Capturing in FreeBSD ##
**[This figure](http://ringmap.googlecode.com/files/FreeBSD_Capturing.png)** demonstrates the standard packet capturing stack in FreeBSD. When packets arrive at a network adapter, the adapter verifies the checksum, extracts the link-layer data and transfers the packets into the RAM using DMA. Next, generates the network adapter an interrupt. This interrupt calls the corresponding interrupt service routine (ISR) of adapter driver. The ISR usually checks only the cause of the interrupt and then, depending  on  number of received packets, plans one or more kernel-threads. The kernel-thread  checks the received packets, allocates structures for packets representation (_[mbufs](http://www.freebsd.org/cgi/man.cgi?query=mbuf&manpath=FreeBSD+8.0-RELEASE)_) and passes packets to TCP/IP-protocol stack and to every **BPF** (one for each application which is capturing). The BPF applies the previously installed filters, and if the packet is accepted it is copied into the BPF-STORE/HOLD double buffer. Now the captured packets can be fetched through _read(2)_ on _/dev/bpf_ or using _libpcap_ functions.


## Network Adapter ##
This section describes how the packet reception works by supported controllers.
In the general case, for all network adapters, packet reception consists of recognizing the presence of a packet on the wire, storing the packet in the receive data FIFO on the adapter and transferring the data to a receive buffer in RAM. The differences between several adapters can appear among others  in DMA and interrupt functionalities.

### GbE Intel Network Adapters ###
#### Supported Controllers: ####
  * Gigabit Ethernet adapters based on the following Intel controller chips:
    * _82540, 82541ER, 82541PI, 82542, 82543, 82544, 82545, 82546, 82546EB, 82546GB, 82547, 82571, 82572 and 82573_

#### Packet Receive ####
The Adapter provide the support for a circular ring of **descriptors**. Adapter uses the descriptors in order to accomplish DMA-operations. The descriptor (see [Figure](http://ringmap.googlecode.com/files/Decriptor_e1000.png)) is a set of data, that includes some predefined information such as the pointer to the buffer in RAM and other fields relevant for a network packet and DMA operation. The software (driver) allocates the array of descriptors in RAM. This allocation must be contiguous in the physical address space. Also driver-software allocates the receive buffers for packets and initializes the descriptors with pointers to these buffers. The address of the first byte of descriptors array must be wrote in the RDBAL/RDBAH registers of adapter. On this way the adapter wil know where the descriptors are placed in RAM and will be able to fetch the descriptors in order to accomplish the DMA-transfers.

The adapter provides two registers **RDT** and **RDH** which are used as _tail_ and _head_ pointers in the descriptors ring (see [figure](http://ringmap.googlecode.com/files/DescriptorRing.png)). The _head_ and _tail_ pointers reference 16-byte blocks of memory where descriptors are saved. As packets arrive, they are stored in the packet-buffers in RAM, corresponding descriptors are updated and the _head_ pointer is incremented by hardware. The _tail_ pointer wil be incremented by software after reading the received packets. When the _head_ pointer is equal to the _tail_ pointer, the ring is empty. Then hardware stops storing packets in RAM until software advances the _tail_ pointer, making more receive buffers available.

# Ringmap Design #
To increase the performance of capturing we want to map packets buffers and some other structures, that are relevant for packet, in the user space. It will save us the copy operations and system calls for accessing the packets while capturing.

# External Links #
  1. [FreeBSD 8.x BPF(4) man page, including description of zero-copy BPF buffers](http://www.FreeBSD.org/cgi/man.cgi?query=bpf&apropos=0&sektion=0&manpath=FreeBSD+8-current&format=html)
  1. [Zero-Copy BPF Buffers](http://www.seccuris.com/documents/whitepapers/20070517-devsummit-zerocopybpf.pdf)
  1. [PCI/PCI-X Family of Gigabit Ethernet Controllers Software Developer’s Manual](http://download.intel.com/design/network/manuals/8254x_GBe_SDM.pdf)