#summary My proposal for Google Summer of Code 2010
#labels Deprecated



# Personal Data #
|Name:|Alexander Fiveg|
|:----|:--------------|
|Email:|afiveg(at)freebsd.org|
|Availability:|30 hours per week|
|Start:|avaliable from 1 May|

<br>

<h1>Bio</h1>
Currently I am a student at the Berlin Institute of Technology, Berlin, Germany. Last<br>
year within the context of the <b>ringmap</b>-Project, I implemented the software components for FreeBSD (kernel- and user-space) for efficient packet capturing in high-speed networks.  While working on this project, I greatly increased my strength in C programming and<br>
my experience with FreeBSD kernel programming, especially with respect to<br>
network drivers.  Moreover, during my studies I have acquired a solid theoretical<br>
knowledge of operating systems and computer networks.<br>
<br>
My practical skills are mainly:<br>
<ul><li>Programming languages: C, Shell, Python<br>
</li><li>Operating systems: Linux, FreeBSD<br>
</li><li>Revision control systems: svn, cvs</li></ul>

<br>

<h1>Mentoring Organization</h1>
The FreeBSD Project<br>
<br>
<h1>Mentors</h1>
<table><thead><th>Name:</th><th>Philip Paeps</th></thead><tbody>
<tr><td>Email:</td><td> philip(at)freebsd.org</td></tr></tbody></table>

<br>

<h1>Project Information</h1>

<h2>Project Title</h2>
<b>Ringmap Capturing Stack for High Performance Packet Capturing in FreeBSD</b>

<h2>Project Description</h2>
Packet capturing in high-speed networks is not an easy task due to the system<br>
limitations such as RAM and system bus throughput. Unfortunately, it is often<br>
impossible to capture every packet arriving at the network interface.  Hardware<br>
may not be the only bottleneck, however, as inefficient software is also to<br>
blame for poor resources usage and the resulting packet loss during capture.<br>
<br>
The new <b>ringmap</b> capturing stack for FreeBSD already allows for efficient capturing at 1GBit/sec on commodity<br>
hardware with very low packet loss and low<br>
system load (under 12%). Under similar conditions, with Intel 1GbE cards<br>
and 64 bytes packets, the standard capturing software of FreeBSD-7.x during capturing generates a system load of up to 100% and results in close to 100% packet loss.<br>
<br>
Similar to "zero-copy BPF" implementation, our idea is to eliminate packet copy operations by using shared memory buffers. However, unlike the "zero-copy BPF" model, ringmap eliminates all packet copies during capturing:<br>
the network adapter's DMA buffer is mapped directly into the user-space. Ringmap also adapts <i>libpcap</i> accordingly to provide user-space applications with access to the captured packets without any additional overhead.<br>
<br>
In the context of the GSoC-2010 I will continue developing the ringmap packet<br>
capturing stack and I plan to achieve the following goals:<br>
<br>
<ul><li>Refactoring ringmap code in order to make it portable to other network adapters<br>
</li><li>Porting ringmap to 10GbE Intel network adapters<br>
</li><li>Porting the ringmap capturing stack to FreeBSD-8<br>
</li><li>Measurements and performance comparison between the standard packet capturing stack of FreeBSD-8 and ringmap.</li></ul>



<h3>What I Plan To Do In Order To Achieve These Goals</h3>

<h4>Code Refactoring and Porting Ringmap To 10GbE Intel Network Adapters</h4>

The ringmap capturing stack is currently based on em-driver and is usable only with<br>
Intel 1Gb Ethernet Adapters. Therefore, the goal is to modify the ringmap<br>
capturing stack for the 10Gb Ethernet Intel network adapters based on 82598 and<br>
82599 processors.<br>
To achieve this goal the next steps are to:<br>
<ul><li>Understand the functionality of 10GbE Intel controllers<br>
</li><li>Understand and analyze the code from <b>ixgbe</b> driver<br>
</li><li>Port the ringmap code to ixgbe</li></ul>

I'm planning at first to split out the existing ringmap code into<br>
hardware-dependent and hardware-independent code. This will first be<br>
done by splitting the data structures and then the functions. This must be done because the present ringmap code was implemented  only for em-driver and only for one<br>
type of Ethernet adapters (based on <b>8254x</b>, <b>82571</b>, <b>82572</b> and <b>82573</b> controllers), and as a result the hardware-dependent and<br>
-independent data is mixed together within the same structures.  Moreover, I have<br>
to extend the existing data structures to make the ringmap code more portable.<br>
<br>
Next, I will follow by porting the <i>ringmap</i> functions to the ixgbe-driver. In order<br>
do it, I will first insert the "hooks" for <i>ringmap</i>
functionalities into the ixgbe-driver code  (through #ifdef). These should include:<br>
<ul><li>Init hook:<br>
<ul><li>Will be placed in the "attach" function of ixgbe driver.<br>
</li></ul></li><li>Uninit hook:<br>
<ul><li>Will be placed in the "detach" function of ixgbe driver<br>
</li></ul></li><li>Interrupt hooks:<br>
<ul><li>Multiple hooks that will be placed in the code of ixgbe that is executed as result of a packet receive interrupt.</li></ul></li></ul>

The first places where I will port the <i>ringmap</i> functions are "attach" and<br>
"detach" functions of ixgbe-driver. In addition to their ordinary tasks, these functions must be able to allocate and deallocate the <i>ringmap</i> data structures properly. After this step I will begin porting most complicated part of the code that has to be executed as a result of interrupt.<br>
<br>
While porting the driver there may appear some difficulties due to the<br>
differences between the 1Gb and 10Gb adapters. In this connection, before<br>
starting the work I must study carefully the functionality of the 10Gb Intel<br>
controller with a strong emphasis of DMA- and interrupt-functionality and<br>
find differences which will impact changes to the driver code.<br>
<br>
<br>
<h4>Porting The Ringmap Capturing Stack To FreeBSD-8</h4>

At the present time, ringmap can be run only on FreeBSD-<b>7.x</b>. The task here<br>
would be to port the existing code base to FreeBSD-<b>8.x</b>. The porting effort<br>
would involve both the driver and libpcap source code.<br>
<br>
<br>
<h4>Measurements And Performance Comparison Between The Standard Packet Capturing Stack Of FreeBSD-8 And Ringmap</h4>

For this part of work I have already deployed a tesbed network of three hosts and I have<br>
implemented the software for tests control and measurements:<br>
<br>
<ol><li><b>Traffic Generator Host</b>. With multiple network interfaces. OS Linux.<br>
<ul><li>Software:<br>
<ul><li>pktgen module to generate network traffic<br>
</li><li>shell-script to control the pktgen module in order to generate traffics with different packet rates and packet lengths.<br>
</li></ul></li></ul></li><li><b>Capturing Host.</b> Os FreeBSD.<br>
<ul><li>Software:<br>
<ul><li><i>libpcap</i>-based application for packet capturing<br>
</li><li>shell-scripts for system load measurement<br>
</li></ul></li></ul></li><li><b>Test Control Host.</b> OS Linux.<br>
<ul><li>Software:<br>
<ul><li>shell-scripts for the central coordination of tests</li></ul></li></ul></li></ol>

<br>

<h2>Project Schedule</h2>

I plan 13 weeks for the whole work .<br>
<br>
<ol><li>Porting ringmap to 10Gb<br>
<ul><li>4 weeks.<br>
</li><li>The bare minimum I want to achieve is the 10GbE ringmap driver for FreeBSD-7. This also is the primary goal of this GSoC-2010 at any cost.<br>
</li></ul></li><li>Functionality Test<br>
<ul><li>2 weeks.<br>
</li><li>Functionality test and debugging of 10Gb ringmap capturing stack.<br>
</li></ul></li><li>Porting ringmap to FreeBSD-8<br>
<ul><li>1-2 weeks.<br>
</li><li>This step will be the port to FreeBSD-8, but it requires step 1 to be finished (at least the coding part, but not necessarily all testing work)<br>
</li></ul></li><li>Functionality Test<br>
<ul><li>1 week.<br>
</li><li>Functionality test and debugging of FreeBSD-8 ringmap capturing stack.<br>
</li><li><b>CODING PART DEADLINE at the end of 9th week</b>
</li></ul></li><li>Performance Test<br>
<ul><li>2 weeks.<br>
</li><li>Conducting performance analysis and evaluation between the standard packet capturing stack of FreeBSD-8 and ringmap. To perform effective performance evaluation, several servers for traffic generation will be used together in order to reach greater-than 1Gibt/sec traffic.<br>
</li></ul></li><li>Documentation<br>
<ul><li>2 weeks.<br>
</li><li>Documentation and cosmetic debugging of ringmap<br>
</li><li><b>PROJECT DEADLINE at the end of 13th week</b></li></ul></li></ol>

<br>

<h2>Support</h2>
The hardware infrastructure and testbed for implementation and testing will<br>
be provided by the research group Intelligent Networks (<a href='http://www.net.t-labs.tu-berlin.de/'>FG INET</a>) at the Berlin Institute of Technology<br>
<br>
<b>The technical contact is:</b><br>
Jan Böttger (Boettger)<br>
Email : jan(at)net.t-labs.tu-berlin.de<br>
phone: +49 (0)30 8353 58516<br>
Berlin Institute of Technology, Berlin, Germany<br>