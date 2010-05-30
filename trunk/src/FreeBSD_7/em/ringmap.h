#ifdef __E1000_RINGMAP__
#include "ringmap_e1000.h"
#endif

/* Minimum distance to be kept between the userrp and RDT to provide a
 * guarantee  to userspace processes that the previous n buffer positions
 * behind userrp will  not be overwritten */
#define RING_SAFETY_MARGIN 	3

/* Max value for number of descriptors (a.k.a. slots in the ringbuffer) is 4096 */
#define SLOTS_NUMBER		2048

/* Prefix for name of device (for example /dev/ringmap_cdev_0 will full name) */
#define RINGMAP_DEVICE 			"ringmap_cdev_"

/* Name of module to be loaded*/
#define MOD_NAME 				"if_ringmap.ko"

/* Messaure statistics for each pkt */
#define EACH_PKT 20

/* Driver have to work only with device wich has the following device ID */
// #define DEV_ID 	0x105E 
#define DEV_ID 	0

struct address {
	bus_addr_t 	phys;
	vm_offset_t	kern;
	vm_offset_t	user;
};

/*
 * This structure represents the ring slot. 
 */
struct ring_slot {
	struct address 	descriptor;
	struct address 	mbuf;
	struct address	packet;


	/* Next fields are for statistics */

	/* Time stamp of packet which placed in the slot */
	struct timeval	ts;

	/* Interrupts number in which kontext the packet was received */
	unsigned long long intr_num;

	/* Packets counter */
	unsigned long long cnt;
};

/*
 * Packet ring buffer
 */
struct ring {

	/*
	 * kernrp - ring HEAD. Should be changed ONLY in driver. And should be
	 * synchronized with the hardware ring HEAD register (RDH).
	 */
	unsigned int kernrp;

	/* 
	 * userrp - ring TAIL. Should be incremented by user space software after
	 * reading the slots with a new received packets
	 */
	unsigned int userrp;

	/* This variable represents the value of Hardware TAIL register */
	unsigned int hw_RDT;

	/* Number of slots (descriptors a.k.a memory areas for frames) */
	unsigned int size;

	/* 
	 * Number of times kernel (hardware) waits for user process. More
	 * specifically, this is the number of times that the write pointer (RDT)
	 * bumps into the slot whose number is (userrp - RING_SAFETY_MARGIN = RDT)
	 *
	 * A.K.A. Hardware is writing faster than the userprocess can read
	 */
	unsigned long long kern_wait_user;

	/* 
	 * Number of times the user process bumps into the RDH.
	 *
	 * A.K.A. User process has read everything there is to read in the ring.
	 */
	unsigned long long user_wait_kern;

	/* Counts number of hardware interrupts */
	unsigned long long interrupts_counter;

	/* Array of slots */
	struct ring_slot slot[SLOTS_NUMBER];
};

#ifdef _KERNEL
struct adapter;

struct ringmap {
	/* Driver's adapter structure */
	struct 	adapter *adapter;

	/* Now only one process can capture from one interface */
	struct 	proc 	*procp;
	struct 	thread 	*td;

	/* Char device for communications between user and kernel spaces */
	struct 	cdev 	*ringmap_dev;

	/* Now only one process can only one time open device */
	uint32_t		open_cnt;
	
	/* How many frames have seen driver in RAM */
	unsigned long long 	pkts_counter;

	/* Our ring that have to be mapped in space of user process */
	struct ring 		ring;
};

#endif /* _KERNEL */

#ifndef _KERNEL

typedef uint64_t u64;

struct e1000_hw_stats {
	u64 crcerrs;
	u64 algnerrc;
	u64 symerrs;
	u64 rxerrc;
	u64 mpc;
	u64 scc;
	u64 ecol;
	u64 mcc;
	u64 latecol;
	u64 colc;
	u64 dc;
	u64 tncrs;
	u64 sec;
	u64 cexterr;
	u64 rlec;
	u64 xonrxc;
	u64 xontxc;
	u64 xoffrxc;
	u64 xofftxc;
	u64 fcruc;
	u64 prc64;
	u64 prc127;
	u64 prc255;
	u64 prc511;
	u64 prc1023;
	u64 prc1522;
	u64 gprc;
	u64 bprc;
	u64 mprc;
	u64 gptc;
	u64 gorc;
	u64 gotc;
	u64 rnbc;
	u64 ruc;
	u64 rfc;
	u64 roc;
	u64 rjc;
	u64 mgprc;
	u64 mgpdc;
	u64 mgptc;
	u64 tor;
	u64 tot;
	u64 tpr;
	u64 tpt;
	u64 ptc64;
	u64 ptc127;
	u64 ptc255;
	u64 ptc511;
	u64 ptc1023;
	u64 ptc1522;
	u64 mptc;
	u64 bptc;
	u64 tsctc;
	u64 tsctfc;
	u64 iac;
	u64 icrxptc;
	u64 icrxatc;
	u64 ictxptc;
	u64 ictxatc;
	u64 ictxqec;
	u64 ictxqmtc;
	u64 icrxdmtc;
	u64 icrxoc;
	u64 cbtmpc;
	u64 htdpmc;
	u64 cbrdpc;
	u64 cbrmpc;
	u64 rpthc;
	u64 hgptc;
	u64 htcbdpc;
	u64 hgorc;
	u64 hgotc;
	u64 lenerrs;
	u64 scvpc;
	u64 hrmpc;
};
#endif



#include <sys/ioccom.h>

/* *************************************
 * IOCTL ' s  system calls             *
 * *************************************/
#define RINGMAP_IOC_MAGIC	'T'

/* Number of descs (a.k.a. slots in ring buffer) */
#define IOCTL_G_DNUM	_IOWR(RINGMAP_IOC_MAGIC, 1, unsigned int)

/* Start capturing. Enable packets receive and interrupts on NIC */
#define IOCTL_ENABLE_RECEIVE	_IO(RINGMAP_IOC_MAGIC, 3)

/* Disable packets receive and interrupts on NIC */
#define IOCTL_DISABLE_RECEIVE	_IO(RINGMAP_IOC_MAGIC, 4)

/* Sleep and wait for new pkts in ring buffer */
#define IOCTL_SLEEP_WAIT		_IO(RINGMAP_IOC_MAGIC, 5)

/* Disable Flow Control */
#define IOCTL_DISABLE_FLOWCNTR	_IO(RINGMAP_IOC_MAGIC, 6) 

/* RDT = (userrp - RING_SAFETY_MARGIN) mod SLOTS_NUMBER */
#define IOCTL_SET_RDT			_IO(RINGMAP_IOC_MAGIC, 2) 

/**********************************************
 *  	Arithmetic in Ring Buffer	
 **********************************************/

#define R_MODULO(a,b)								\
	( ((unsigned int)(a)) % ((unsigned int)(b)) )

/* Distance from "a" to "b" in ring: r = (b-a) mod (size) */
#define RING_DISTANCE(r,a,b,size)										\
	(r) = R_MODULO(((b) - (a)), (size));

#define R_DISTANCE(a,b,size)		\
	(R_MODULO((b)-(a), (size)))

/* Distance from KERN pointer to USER pointer */
#define KERN_TO_USER_DIST(r, ringp)										\
	do {																\
		RING_DISTANCE((r), (ringp)->kernrp, (ringp)->userrp, SLOTS_NUMBER);	\
		if ((r) == 0) (r) = SLOTS_NUMBER;								\
	} while (0);

/* Distance from USER pointer to KERN pointer */
#define USER_TO_KERN_DIST(r, ringp)										\
	do {																\
		RING_DISTANCE((r), (ringp)->userrp, (ringp)->kernrp, SLOTS_NUMBER);	\
	} while(0);

/* Distance from KERN to USER pointer */
#define KERN_TO_USER_RING_DISTANCE(ringp)								\
	(((ringp)->userrp == (ringp)->kernrp) ?  SLOTS_NUMBER : R_DISTANCE((ringp)->kernrp, (ringp)->userrp, SLOTS_NUMBER))

/* Distance from USER to KERN pointer */
#define USER_TO_KERN_RING_DISTANCE(ringp)	\
	(R_DISTANCE((ringp)->userrp, (ringp)->kernrp, SLOTS_NUMBER))

/* Increment of USER pointer. (KERN pointer is incremented by Hardware) */
#define INC_USER_POINTER(ringp)											\
	((ringp)->userrp) = R_MODULO(((ringp)->userrp + 1), SLOTS_NUMBER);

/* Add "a" to USER pointer */
#define ADD_USER_POINTER(ringp, a)										\
	((ringp)->userrp) = R_MODULO(((ringp)->userrp + a), SLOTS_NUMBER);

/* Next Operationts are only in Kern usefull because they set hardware registers */
#ifdef _KERNEL

/* Read value from RDH, set RDT = RDH - RING_SAFETY_MARGIN */
#define INIT_RING_AND_REGISTERS(ringp, adapter)							\
	do{																	\
		(ringp)->kern_wait_user 	= 0;								\
		(ringp)->user_wait_kern 	= 0;								\
		(ringp)->interrupts_counter	= 0;								\
		(ringp)->size   = SLOTS_NUMBER;							\
		(ringp)->kernrp = RINGMAP_HW_READ_REG(&(adapter)->hw, E1000_RDH(0));	\
		(ringp)->userrp = (ringp)->kernrp;								\
		RINGMAP_HW_WRITE_REG(&(adapter)->hw, E1000_RDT(0), R_MODULO(((ringp)->userrp) - RING_SAFETY_MARGIN, SLOTS_NUMBER));	\
	}while(0);

#endif

/*
 *		D E B U G    O U T P U T
 */
#ifndef IOCTL_DEB
#define IOCTL_DEB 0
#else 	
#define IOCTL_DEB 1
#endif

#ifndef INTR_DEB
#define INTR_DEB 0
#else 	
#define INTR_DEB  1
#endif

#ifndef __RINGMAP_DEB
#define __RINGMAP_DEB 0
#endif

#define RINGMAP_PREFIX 	"--> RINGMAP: " 
#define ERR_PREFIX 		"--> RINGMAP ERROR: " 
#define WARN_PREFIX 	"--> RINGMAP WARN: "

#define RINGMAP_ERROR(x) 	printf("---> RINGMAP ERROR: [%s]: "  #x "\n", __func__);
#define RINGMAP_IOCTL(x)	if (IOCTL_DEB) 	printf(" --> RINGMAP IOCTL: " #x "\n");
#define RINGMAP_INTR(x)  	if (INTR_DEB) 	printf("[%s] --> RINGMAP INTR: " #x "\n", __func__);
#define RINGMAP_FUNC_DEBUG(x) if (__RINGMAP_DEB) printf("[%s] --> RINGMAP FUNC:" #x "\n", __func__);
#define RINGMAP_OUTPUT(x) if (__RINGMAP_DEB) printf("--> RINGMAP: [%s]: "  #x "\n", __func__);
#define RINGMAP_WARN(x) 	if (__RINGMAP_DEB) printf("--> WARN: [%s]: "  #x "\n", __func__);

#ifdef _KERNEL
#define RINGMAP_PRINT_DESC(i)	\
		printf("[%s] - DESC INFO: desc_num=%d, status=0x%X, pktlen=%d\n[%s] - ADDRESSES: pkt_virt=0x%X (kern), pkt_phys=0x%X\n", \
				__func__, 												\
				i, 														\
				(unsigned int)(adapter->rx_desc_base[i].status) & 255, 	\
				adapter->rx_desc_base[i].length,						\
				__func__, 												\
				(unsigned int)adapter->rx_buffer_area[i].m_head->m_data,\
				(unsigned int)adapter->rx_desc_base[i].buffer_addr);

#define FIVEG_PRINT_SOME_BYTES_FROM_PKT(i)					\
   printf("[%s] SOME BYTES FROM PKT: %hhX %hhX %hhX %hhX %hhX\n", __func__,	\
		   adapter->rx_buffer_area[i].m_head->m_data[0],	\
		   adapter->rx_buffer_area[i].m_head->m_data[1],	\
		   adapter->rx_buffer_area[i].m_head->m_data[16],	\
		   adapter->rx_buffer_area[i].m_head->m_data[32],	\
		   adapter->rx_buffer_area[i].m_head->m_data[59]);	
#else 
#define FIVEG_PRINT_SOME_BYTES_FROM_PKT(pktp)					\
   printf("[%s] SOME BYTES FROM PKT: %hhX %hhX %hhX %hhX %hhX\n", __func__,	\
		   pktp[0],		\
		   pktp[1],		\
		   pktp[16],	\
		   pktp[32],	\
		   pktp[59]);	
#endif
