struct ringmap_functions;
struct address; 
struct ring_slot;
struct ring;

struct capt_object {
	/* Slots number in the ring buffer */
	struct 	ringmap *rm;
	struct 	thread *td;

	struct 	ring *ring;

	/* 
	 * Pointers to the structures allocated in the generic driver for accesisng
	 * hardware registers related to the rx/tx queues associated with our 
	 * capturing object. 
	 */
	void *hw_rx_ring, *hw_tx_ring;

	/*
	 * Pointer to the arrays allocated in the generic driver for accessing 
	 * rx/tx buffers and descriptors.
	 */
	void *rx_buffers, *tx_buffers, *rx_desc_base, *tx_desc_base;

	/* Packet filtering code. */
	struct bpf_insn *fcode;

	/* 
	 * Source of interrupts affecting our capturing object. It can be adapter,
	 * device or queue structure.
	 */
	void *intr_context;

	/* If the thread can sleep */
	int volatile can_sleep;

	/* Let's concatenate our objects */
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
	 * This function should be called from ISR. It should contain 
	 * the very fast executable operations (don't sleep!). 
	 */
	void (*isr)(void *);

	/*
	 * This function should be calld from delayed interrupt 
	 * routine. It can contain operations that must not be 
	 * very fast (can sleep).
	 */
	struct capt_object * (*delayed_isr)(void *, struct ringmap *);

	/*
	 * The native driver usually has cycle for checking the packets DMAed in
	 * the RAM from network controller. per_packet_iteration() should be called
	 * from that cycle, so it should be called once per packet.
	 */
	void (*per_packet_iteration)(struct capt_object *, int);

	/* Returns value of TAIL controller register */
	unsigned int (*get_tail)(void *hw_ring);

	/* Set value into the TAIL controller register */
	void (*set_tail)(unsigned int val, void *hw_ring);

	/* Returns value of HEAD controller register */
	unsigned int (*get_head)(void *hw_ring);

	/* Returns pointer to mbuf from array allocated in the generic driver */
	struct mbuf * (*get_mbuf)(void *buffer_area, unsigned int num);

	/* Returns pointer to the packet buffer: mbuf->m_mhead->m_data */
	vm_offset_t (*get_packet)(void *buffer_area, unsigned int num);

	/* 
	 * Associate the capturing objec with a hardware queue (a.k.a ring). Also 
	 * in this function the pointers for accessing hardware registers and 
	 * rx/tx buffers should be set.  
	 */
	int (*set_queue)(struct capt_object *, unsigned int);

	/* 
	 * This function is responsible for packet filtering. In case the packet is
	 * accepted by filter the slot[int].filtered should be set to 1. See
	 * "slot"-structure in ringmap.h
	 */
	void (*pkt_filter)(struct capt_object *, int);

	/* Retunrns the number of available queues */
	int (*get_queuesnum)(void);

	/* 
	 * Set timestamp for packet placed in the slot. If ts != NULL set ts as
	 * timestamp. Else compute timestamp calling getmicrotime(9) or take
	 * timestamp from descriptor if controller supports timestamping.
	 */
	void (*set_timestamp)(struct ring_slot *slot, struct timeval *ts);

	void (*receive_disable)(struct ringmap *rm);
	void (*intr_disable)(struct ringmap *rm);

	void (*receive_enable)(struct ringmap *rm);
	void (*intr_enable)(struct ringmap *rm);

	/* Slots number in the ring buffer */
	void (*set_slotsnum)(struct ring *);
	unsigned int (*get_slotsnum)(struct ring *);
};


/* MUTEX */
#define	RINGMAP_LOCK_INIT(rm, _name) 	\
	mtx_init(&(rm)->ringmap_mtx, _name, "RINGMAP Lock", MTX_DEF)
#define	RINGMAP_LOCK_DESTROY(rm)		mtx_destroy(&(rm)->ringmap_mtx)
#define	RINGMAP_LOCK(rm)			mtx_lock(&(rm)->ringmap_mtx)
#define	RINGMAP_TRYLOCK(rm)		mtx_trylock(&(rm)->ringmap_mtx)
#define	RINGMAP_UNLOCK(rm)		mtx_unlock(&(rm)->ringmap_mtx)


/* Debug */
#define CAPT_OBJECT_DEB(co)									\
	if (__RINGMAP_DEB) {									\
		if (co != NULL) {									\
			printf("\n===  co->td->proc->pid: %d\n", 		\
					co->td->td_proc->p_pid);				\
			printf("===  Ring Kernel Addr: %p\n", 			\
					(void *)co->ring);				\
			PRINT_RING_PTRS(co->ring);						\
		} else {											\
			RINGMAP_WARN(NULL pointer: capturing object);	\
		}													\
	};
