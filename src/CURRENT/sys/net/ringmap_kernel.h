struct ringmap_functions;

struct capt_object {
	struct 	ringmap *rm;
	struct 	thread *td;

	struct 	ring *ring;
	void 	*que;

	struct bpf_insn *fcode;

	/* Source of interrupts affecting our capturing object */
	void *intr_context;

	SLIST_ENTRY(capt_object) objects;
};


/*
 * This structure will be visible only in the kernel. It contains 
 * the pointers to the ring that should be mapped in user-space, 
 * to the functions for accessing the ring and for accessing to the 
 * device and driver structures
 */
struct ringmap {
	/* Device structure */
	device_t dev;

	/* Char device for communications between user and ringmap */
	struct 	cdev 	*cdev;

	/* Number of threads that opened cdev. 
	 * A.K.A. number of capturing objects  */
	uint32_t volatile open_cnt;
	
	/* Hardware dependent functions */
	struct ringmap_functions *funcs;

	struct mtx	ringmap_mtx;

	/* Head of the list of capturing objects */
	SLIST_HEAD(object_list, capt_object) object_list;

	/* Next ringmap in the list */
	SLIST_ENTRY(ringmap) entries;
};

/* SLIST of ringmap structures */
SLIST_HEAD(ringmap_global_list, ringmap);

struct ringmap_functions {

	/*
	 * This function should be calld from ISR. It should contain 
	 * the very fast executable operations (don't sleep!). 
	 */
	void (*isr)(void *);

	/*
	 * This function should be calld from delayed interrupt 
	 * function. It can contain operations that must not be 
	 * very fast (can sleep).
	 */
	void (*delayed_isr)(void *, struct ringmap *);

	/*
	 * The native driver should have cycle for checking the packets that was
	 * transfered in the RAM from network adapter. per_packet_iteration()
	 * should be called from that cycle, so it will be called per packet.
	 */
	void (*per_packet_iteration)(struct ringmap *, int);

	/* 
	 * Next functions synchronize the tail and head hardware registers
	 * with head and tail software varibles visible in both  
	 * kernel- and user-space. 
	 *
	 * Synchronisation rules:
	 * 1. SYNC_HEAD: HARDWARE_HEAD => SOFTWARE_HEAD
	 * 		set value from hardware HEAD register into the software visible
	 * 		HEAD-variable: ring->kernrp.  The User-space process shouldn't
	 * 		touch the ring->kernrp variable. Only hardware increment the value
	 * 		in the HEAD register onto adapters chip while receiving new
	 * 		packets, and only driver (kernel) synchronizes then hardware HEAD
	 * 		with ring->kernrp.
	 *
	 * 2. SYNC_TAIL: SOFTWARE_TAIL => HARDWARE_TAIL
	 *		set value from software TAIL-variable: ring->userrp into the
	 *		hardware TAIL-register. Hardware shouldn't change the content of
	 *		TAIL-register.  Software after reading one packet in RAM
	 *		increments the value of ring->userrp. Kernel will check this value
	 *		(it is mapped - visible in kernel and user) and set it into the
	 *		hardware TAIL-register.
	 */
	void (*sync_tail)(struct capt_object *);
	void (*sync_head)(struct capt_object *);

	/* Initialize the ring slot */
	int (*set_slot)(struct capt_object *, unsigned int);

	/* 
	 * Associate the capturing objec with a hardware queue. Usable ONLY
	 * on controllers supported multiple queues 
	 */
	int (*set_queue)(struct capt_object *, unsigned int);

	/* 
	 * This function is responsible for packet filtering. In case the packet is
	 * accepted by filter the slot[int].filtered should be set to 1. See
	 * "slot"-structure in ringmap.h
	 */
	void (*pkt_filter)(struct capt_object *, int);

	/* 
	 * Set timestamp for packet placed in the slot. If ts != NULL set ts as
	 * timestamp. Else compute timestamp calling getmicrotime(9) or take
	 * timestamp from descriptor if controller supports timestamping.
	 */
	void (*set_timestamp)(struct ring_slot *slot, struct timeval *ts);
};

/* MUTEX */
#define	RINGMAP_LOCK_INIT(rm, _name) 	\
	mtx_init(&(rm)->ringmap_mtx, _name, "RINGMAP Lock", MTX_DEF)
#define	RINGMAP_LOCK_DESTROY(rm)		mtx_destroy(&(rm)->ringmap_mtx)
#define	RINGMAP_LOCK(rm)			mtx_lock(&(rm)->ringmap_mtx)
#define	RINGMAP_TRYLOCK(rm)		mtx_trylock(&(rm)->ringmap_mtx)
#define	RINGMAP_UNLOCK(rm)		mtx_unlock(&(rm)->ringmap_mtx)
