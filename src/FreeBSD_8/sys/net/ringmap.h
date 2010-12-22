#ifdef _KERNEL
#include "ringmap_kernel.h"
#endif


/* 
 * value for number of descriptors (a.k.a. slots in the ringbuffer)
 */
#define SLOTS_NUMBER		64

/* 
 * Prefix for name of device (for example /dev/ringmap_em0 will full name) 
 * currently not used, but it will!
 */
#define RINGMAP_DEVICE 		"ringmap"

/*
 * Default queue number. For multiqueue. Currently not used!
 */
#define DEFAULT_QUEUE	1

/*   1 - enable time stamping in the driver
 * 	TODO: eliminate it.
 */
#define RINGMAP_TIMESTAMP 1

/* 
 * TODO: should not be as macro
 * Max number of user threads that capture using ringmap 
 */
#define RINGMAP_MAX_THREADS 8

/*
 * TODO:
 * Somehow I don't like this addres stuff. We can get by whithout it.
 * I am looking how to eliminate it. 
 */
struct address {
	vm_paddr_t 	 phys;
	vm_offset_t	 kern;
	vm_offset_t	 user;
};

/*
 * This structure represents the ring slot. "mbuf" represents the kernel view
 * of packet. The "packet" represents the buffer where the packet data is
 * placed. Both are of type 'struct address'. Struct 'address' contains three
 * addresses: physical-, kernel- and user-address. We need to store the
 * physical addresses to be able to do memory mapping. 
 */
struct ring_slot {

	struct address 	mbuf;
	struct address	packet;

	/* 1 - if accepted by (bpf) filter */
	int volatile filtered;

	/* 1 if accepted by driver and contains no errors */
	int volatile is_ok;

	/* Time stamp of packet stored in this slot */
	struct timeval	ts;

	/* Interrupts number in which kontext the packet was received */
	unsigned long long intr_num;

	/* Packets counter */
	unsigned long long cnt;
};

/*
 * This structure represents the packets ring buffer. The structure has to be
 * mapped into the user-space to be visible and accessible from the user
 * capturing application. The ring contains the pointer to SLOTs array. Each
 * SLOT represents one packet. Additionaly, the structure contains ring-HEAD
 * (kernrp) and ring-TAIL (userrp) poiners. 
 */ 
struct ring {

	/*
	 * kernrp - ring HEAD. Must be modified ONLY in the driver through
	 * synchronizing it with ring-HEAD controller register. Adapter increments
	 * the value in its HEAD-register after storing the incomming packets in
	 * the RAM. Then the value of HEAD-register should be written into the
	 * kernrp.
 	 */
	unsigned int volatile kernrp;

	/* 
	 * userrp - ring TAIL. Must be modified only in user space  after reading a
	 * slot with a new received packet. The driver, while executing ISR shoud
	 * check userrp and set this value in the adapter-TAIL-register.
	 */
	unsigned int volatile userrp;

	/* Number of slots (descriptors a.k.a memory areas for frames) */
	int size;

	/* Values from adapters statistic registers. Currently is not used */
	struct address 	hw_stats;

	/* 
	 * Number of times kernel (hardware) waits for user process. More
	 * specifically, this is the number of times that the write pointer (HEAD)
	 * bumps into the slot whose number is stored in TAIL register
	 *
	 * A.K.A. Hardware is writing faster than the userprocess can read
	 */
	unsigned long long kern_wait_user;

	/* 
	 * Number of times the TAIL bumps into the HEAD.
	 *
	 * A.K.A. User process has read everything there is to read in the ring.
	 */
	unsigned long long user_wait_kern;

	/* 
	 * Number of received packets. This variable should be changed  only in
	 * user-space. We want to count the packets, that was seen by user-space
	 * process. I am not sure we are need it :( 
	 */
	unsigned long long pkt_counter;

	struct timeval	last_ts;

	unsigned long long intr_num;

	/* Array of slots (A.K.A packet buffers) */
	struct ring_slot slot[SLOTS_NUMBER];
}; 

/* For libpcap. To be set in the pcap structure */
struct pcap_ringmap {
	int cdev_fd;
	int status;
	int flags;
	struct ring *ring;
};


/** 
 ** IOCTLs
 **/
#define RINGMAP_IOC_MAGIC	'T'

/* 
 * Should call the function that synchronizes hardware TAIL 
 * with SW_TAIL(ring)
 */
#define IOCTL_SYNC_TAIL			_IO(RINGMAP_IOC_MAGIC, 1) 

/* Synchronize both head and tail */
#define IOCTL_SYNC_HEAD_TAIL	_IO(RINGMAP_IOC_MAGIC, 2) 

/* 
 * Disable interrupts on NIC. In some cases it is safe 
 * to disable interrupts. 
 */
#define IOCTL_DISABLE_INTR		_IO(RINGMAP_IOC_MAGIC, 3)

/* Enable interrupts on NIC */
#define IOCTL_ENABLE_INTR		_IO(RINGMAP_IOC_MAGIC, 4)

/* 
 * Sleep and wait for new pkts in ring buffer. By this 
 * system call should the user-space process be blocked 
 * and should be awoken from ISR or delayed ISR after the  
 * new packets was received. Additionaly, in kontext of this
 * syscall hardware HEAD and TAIL registers should be 
 * synchronized with ring->kernerp and ring->userrp
 */
#define IOCTL_SLEEP_WAIT		_IO(RINGMAP_IOC_MAGIC, 5)

/*
 * Set filter programm for packet filtering 
 */
#define IOCTL_SETFILTER			_IOW(RINGMAP_IOC_MAGIC, 6, struct bpf_program)

/*
 * Returns the number of available queues (A.K.A. rings)
 */
#define IOCTL_GETQUEUE_NUM		_IOR(RINGMAP_IOC_MAGIC, 7, int)

/*
 * Associate user-space capturing process with a queue
 */
#define IOCTL_ATTACH_RING		_IOW(RINGMAP_IOC_MAGIC, 8, int)



/**********************************************
 *  	Arithmetic in ring buffer	
 **********************************************/

/* HEAD and TAIL ring pointers */
#define SW_TAIL(ringp)	((ringp)->userrp)
#define SW_HEAD(ringp)  ((ringp)->kernrp)

#define RING_MODULO(a,b)						\
	(((unsigned int)(a)) % ((unsigned int)(b)))

/* Distance from "a" to "b" in ring: r = (b-a) mod (size) */
#define RING_DISTANCE(a,b,size)		\
	(RING_MODULO((b)-(a), (size)))

#define R_MODULO(a)					\
	RING_MODULO((a), (SLOTS_NUMBER))

#define R_DISTANCE(a, b)					\
	RING_DISTANCE((a), (b), (SLOTS_NUMBER))

/* Distance from head to tail in the ring */
#define SW_HEAD_TO_TAIL_DIST(ringp)				\
	R_DISTANCE(SW_HEAD(ringp), SW_TAIL(ringp))

/* Distance from tail to head in the ring */
#define SW_TAIL_TO_HEAD_DIST(ringp)							\
	((SW_TAIL(ringp) == SW_HEAD(ringp)) ? SLOTS_NUMBER : 	\
	R_DISTANCE(SW_TAIL(ringp), SW_HEAD(ringp)))

#define INCR_TAIL(ringp)							\
	(SW_TAIL(ringp)) = R_MODULO(SW_TAIL(ringp) + 1);

#define RING_IS_EMPTY(ringp)				\
	((SW_TAIL_TO_HEAD_DIST(ringp)) == 1)

#define RING_NOT_EMPTY(ringp)				\
	((SW_TAIL_TO_HEAD_DIST(ringp)) > 1)

#define RING_IS_FULL(ringp)					\
	((SW_HEAD_TO_TAIL_DIST(ringp)) == 0)


/**
 ** Access to the ring members 
 **/
#define RING_SLOT(ringp, i)					\
	((ringp)->slot[(i)])

#define TAIL_SLOT(ringp)					\
	RING_SLOT((ringp), (SW_TAIL((ringp))))

#define TAIL_PACKET(ringp)					\
	TAIL_SLOT(ringp).packet

#define U_MBUF(ringp, i)					\
	RING_SLOT((ringp), (i)).mbuf.user

#define K_MBUF(ringp, i)					\
	RING_SLOT((ringp), (i)).mbuf.kern

#define U_PACKET(ringp, i)					\
	RING_SLOT((ringp), (i)).packet.user

#define K_PACKET(ringp, i)					\
	RING_SLOT((ringp), (i)).packet.kern

/**
 **		DEBUG OUTPUT
 **/
#ifndef RINGMAP_IOCTL_DEB
#define RINGMAP_IOCTL_DEB 0
#else 	
#define RINGMAP_IOCTL_DEB 1
#endif

#ifndef RINGMAP_INTR_DEB
#define RINGMAP_INTR_DEB 0
#else 	
#define RINGMAP_INTR_DEB  1
#endif

#ifndef __RINGMAP_DEB
#define __RINGMAP_DEB 0
#else
#define __RINGMAP_DEB 1
#endif

#define RINGMAP_PREFIX 	"--> RINGMAP: " 
#define ERR_PREFIX 		"--> RINGMAP ERROR: " 
#define WARN_PREFIX 	"--> RINGMAP WARN: "
#define IOCTL_PREFIX 	"--> RINGMAP IOCTL: "
#define INTR_PREFIX 	"--> RINGMAP INTR: "

#define RINGMAP_ERROR(x) 	\
	printf(ERR_PREFIX "[%s]: "  #x "\n", __func__);

#define RINGMAP_IOCTL(x)	\
	if (RINGMAP_IOCTL_DEB) 	\
		printf(IOCTL_PREFIX "[%s] " #x "\n", __func__);

#define RINGMAP_INTR(x)  							\
	if (RINGMAP_INTR_DEB) 							\
		printf(INTR_PREFIX "[%s] " #x "\n", __func__);

#define RINGMAP_FUNC_DEBUG(x)	\
	if (__RINGMAP_DEB) 			\
		printf(RINGMAP_PREFIX "[%s] " #x "\n", __func__);

#define RINGMAP_WARN(x) 	\
	if (__RINGMAP_DEB)		\
		printf(WARN_PREFIX"[%s]: "  #x "\n", __func__);

#define PRINT_PKT_BYTES(pktp, i)										\
   printf("=+= [%s] SOME BYTES FROM PKT: %hhu %hhu %hhu %hhu %hhu %hhu %hhu\n", 	\
	   __func__, pktp[0], pktp[1], pktp[2], pktp[30], pktp[31], pktp[32], pktp[33]);

#define PACKET_ADDR_DEB(ring, i)	\
	if (__RINGMAP_DEB) {							\
		printf("=+= packet.user=0x%X, packet.phys=0x%X, packet.kern=0x%X\n",\
		(unsigned int)ring->slot[i].packet.user, 	\
		(unsigned int)ring->slot[i].packet.phys, 	\
		(unsigned int)ring->slot[i].packet.kern);	\
	};

#define PRINT_MBUF_ADDR(ring, i)	\
	do {							\
		printf("=+= mbuf.user=0x%X, mbuf.phys=0x%llX, mbuf.kern=0x%X\n",  \
		(unsigned int)ring->slot[i].mbuf.user,			\
		(long long unsigned int)ring->slot[i].mbuf.phys,\
		(unsigned int)ring->slot[i].mbuf.kern);			\
	} while (0);

#define PRINT_SLOT(ring, i)												\
	if ((ring) != NULL) { 							\
		printf("\n=+= ==================================\n");			\
		printf("=+= Slot Number: %d \n", (i));							\
		printf("=+= Intrr num:  %llu\n", (ring)->slot[(i)].intr_num);	\
		printf("=+= Time stamp: %llu\n", 								\
		(unsigned long long)(((ring)->slot[(i)].ts.tv_sec*1000000 + 	\
				(ring)->slot[(i)].ts.tv_usec)));						\
		printf("=+= Accepted: %d\n", (ring)->slot[(i)].is_ok);			\
		printf("=+= -------------------------\n");						\
		PRINT_MBUF_ADDR(ring, i);										\
		PACKET_ADDR_DEB(ring, i);										\
		printf("=+= -------------------------\n");						\
		printf("=+= ==================================\n\n");			\
	};

#define PRINT_TAIL(ring)				\
	printf("=+= [%s] tail = %d\n", __func__, SW_TAIL(ring));	

#define PRINT_HEAD(ring)				\
	printf("=+= [%s] head = %d\n", __func__, SW_HEAD(ring));

#define PRINT_SLOT_DEB(ring, i)		\
	if (__RINGMAP_DEB) {			\
		PRINT_SLOT((ring), (i))		\
	};

#define PRINT_RING_PTRS(ring)		\
	do {							\
		PRINT_TAIL(ring)			\
		PRINT_HEAD(ring)			\
	} while (0);

#define RING_PTRS_DEB(ring)			\
if (__RINGMAP_DEB) {				\
		PRINT_RING_PTRS(ring)		\
};

#define PKT_BYTES_DEB(pktp, i)			\
if (__RINGMAP_DEB) {					\
		PRINT_PKT_BYTES(pktp, i);		\
};
