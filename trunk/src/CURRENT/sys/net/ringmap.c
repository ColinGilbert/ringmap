#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>
#include <sys/bus.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/ioccom.h>

#include <machine/bus.h>
#include <machine/atomic.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>

#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#include <dev/pci/pcivar.h>   /* For pci_get macros! */
#include <dev/pci/pcireg.h>

#include "ringmap.h"

struct ringmap * ringmap_attach (device_t, struct ringmap_functions *);
int ringmap_detach (struct ringmap *rm);
void ringmap_close_cb (void *data);
void clear_capt_object(void *);
struct ringmap * cdev2ringmap(struct cdev *);
struct ringmap * dev2ringmap(device_t);
void ringmap_bpf_filter(struct capt_object *, int);
void per_packet_iteration(struct capt_object *, int );
struct capt_object * ringmap_delayed_isr(void *context, struct ringmap *rm);
int set_slot(struct capt_object *co, unsigned int slot_num);
void ringmap_timestamp(struct ring_slot *slot, struct timeval *ts);

d_open_t	ringmap_open;
d_close_t	ringmap_close;
d_ioctl_t	ringmap_ioctl;
d_read_t	ringmap_read;
d_mmap_single_t ringmap_mmap_single;

static struct cdevsw ringmap_devsw = {
	.d_version 	= D_VERSION,
	.d_open 	= ringmap_open,
	.d_close 	= ringmap_close,
	.d_ioctl	= ringmap_ioctl,
	.d_read		= ringmap_read,		/* Tell to user ring physical addr */
	.d_mmap_single = ringmap_mmap_single,	/* Doesn't work yet */
	.d_name 	= "ringmap_cdev"
};

static struct ringmap_global_list ringmap_list_head = 
		SLIST_HEAD_INITIALIZER(ringmap_list_head);


/*
 * The function should be called from the attach function of generic network
 * driver.  Here the ringmap structure is allocated and the character special
 * device for the communication with user is created. Also default ringmap
 * functions are set.
 */
struct ringmap *
ringmap_attach(device_t dev, struct ringmap_functions *rf) 
{	
	struct ringmap *rm = NULL;

	RINGMAP_FUNC_DEBUG(begin);

	/* Allocate ringmap */
	MALLOC(rm, struct ringmap *, sizeof(struct ringmap), 
			M_DEVBUF, (M_ZERO | M_WAITOK));
	if (rm == NULL) { 
		RINGMAP_ERROR(Can not allocate space for ringmap structure);
		return (NULL);
	}

	/* 
	 * Create character special device for communication with user-space. The
	 * user-space process wich want to capture packets first opens this device.
	 * Then, by syscalls on this device it will: 
	 * - 	get physical adresses of packet buffers for mapping them in its 
	 * 		virtual memory
	 * -	controll packet capturing: start, stop, sleep to wait for packets.
	 */
	rm->cdev = make_dev(&ringmap_devsw, device_get_unit(dev),
						UID_ROOT, GID_WHEEL, 0666, 
						device_get_nameunit(dev));
	if (rm->cdev == NULL) {
		RINGMAP_ERROR(Can not create character device);
		FREE(rm, M_DEVBUF);
		return (NULL);
	}

	/* Set the hardware and driver speciffic functions */
	rm->funcs = rf;

	/* Store interface device structure in ringmap */
	rm->dev = dev;

	/* 
	 * Initialize the list of capturing objects. Each object represents the
	 * thread that capture traffic and its ring.
	 */
	SLIST_INIT(&rm->object_list);

	/* Insert ringmap structure into the list */
	SLIST_INSERT_HEAD(&ringmap_list_head, rm, entries);

	/* Init the mutex for protecting our data */
	RINGMAP_LOCK_INIT(rm, device_get_nameunit(dev));

	/* 
	 * Set default functions if the generic driver's specific functions are not
	 * set.
	 */
	if (rm->funcs->delayed_isr == NULL)
		rm->funcs->delayed_isr = ringmap_delayed_isr;

	if (rm->funcs->per_packet_iteration == NULL)
		rm->funcs->per_packet_iteration = per_packet_iteration;

	if (rm->funcs->set_timestamp == NULL)
		rm->funcs->set_timestamp = ringmap_timestamp;

	RINGMAP_FUNC_DEBUG(end); 

	/* 
	 * Return ringmap pointer to the generic driver. Generic driver should
	 * store the pointer in the adapter structure in order to be able to access
	 * ringmap.
	 */
	return (rm);	
} 


/* 
 * Should be called from driver's detach function. 
 */
int
ringmap_detach(struct ringmap *rm)
{
	struct capt_object *co = NULL;

	RINGMAP_FUNC_DEBUG(start);
	
	if (rm == NULL) {
		RINGMAP_WARN(Can not get pointer to ringmap structure);
		return (-1);
	}
	
	/* Remove all capturing objects associated with ringmap */
    while (!SLIST_EMPTY(&rm->object_list)) {
	    co = SLIST_FIRST(&rm->object_list);
	    clear_capt_object((void *)co);
    }

	RINGMAP_LOCK(rm);
	/* To be sure */
	if (!SLIST_EMPTY(&rm->object_list)) {
		RINGMAP_WARN(There are still active capturing objects);
	}
	/* Destroy char device associated with ringmap */
	if (rm->cdev != NULL)
		destroy_dev(rm->cdev);

	/* And remove ringmap from global list */
	SLIST_REMOVE(&ringmap_list_head, rm, ringmap, entries);
	RINGMAP_UNLOCK(rm);

	RINGMAP_LOCK_DESTROY(rm);
	FREE(rm, M_DEVBUF);

	RINGMAP_FUNC_DEBUG(end);

	return (0);
}


/*
 * This function is called as result of open(2). Here we allocate the memory
 * for the new ring and capt_object structure (so called capturing object).
 * Capturing object represents a thread with its ring.
 */
int
ringmap_open(struct cdev *cdev, int flag, int otyp, struct thread *td)
{
	int err = 0, i=i;
	struct ringmap *rm = NULL;
	struct ring *ring = NULL;
	struct capt_object *co = NULL;

	RINGMAP_FUNC_DEBUG(start);

#if (__RINGMAP_DEB)
	printf(RINGMAP_PREFIX"[%s] pid = %d\n", __func__, td->td_proc->p_pid);
#endif 
	rm = cdev2ringmap(cdev);
	if ( rm == NULL ) {
		RINGMAP_ERROR(Could not get the pointer to ringmap structure);
		return (EIO);
	}

	RINGMAP_LOCK(rm);

	/* TODO: set max number of threads in the ringmap struct as a member */
	if (rm->open_cnt == RINGMAP_MAX_THREADS) {
		RINGMAP_ERROR(Can not open device!);
		err = EIO; goto out;
	}

	/* First stop receive and interupts while we allocate our data */
	rm->funcs->receive_disable(rm);
	rm->funcs->intr_disable(rm);
	// pause("wait", hz); 

	/* Only ONE open() per thread */
	SLIST_FOREACH(co, &rm->object_list, objects) {
		if (co->td == td) {
			RINGMAP_ERROR(Device is opened!);
			err = EIO; goto out;
		}
	}

	/* 
	 * Allocate memory for ring structure. Use contigmalloc(9) to get
	 * PAGE_SIZE alignment that is needed for memory mapping.  
	 */
	ring = (struct ring *) contigmalloc (sizeof(struct ring), 
			M_DEVBUF, M_ZERO, 0, -1L, PAGE_SIZE, 0);
	if (ring == NULL) {
		RINGMAP_ERROR(Can not allocate space for ring);
		err = EIO; goto out;
	}
	ring->size = SLOTS_NUMBER;

	/* 
	 * create the capturing object wich will represent current thread and
	 * its packets ring 
	 */
	MALLOC(co, struct capt_object *, 
			sizeof(struct capt_object), M_DEVBUF, (M_ZERO | M_WAITOK));
	if ( co == NULL ) {
		contigfree(ring, sizeof(struct ring), M_DEVBUF);
		err = EIO; goto out;
	}
	co->ring = ring;
	co->td = td;
	co->rm = rm;

	SLIST_INSERT_HEAD(&rm->object_list, co, objects);

	/* 
	 * Store capturing object as private data. So we can access our capturing
	 * object in other syscalls, e.g. read, close, etc... 
	 */
	if (devfs_set_cdevpriv((void *)co, clear_capt_object)) {
		RINGMAP_ERROR(Can not set private data!);
		contigfree(ring, sizeof(struct ring), M_DEVBUF);
		FREE(co, M_DEVBUF);
		err = EIO; goto out;
	}

	++rm->open_cnt;
	CAPT_OBJECT_DEB(co);
out:
	rm->funcs->intr_enable(rm);
	rm->funcs->receive_enable(rm);

	RINGMAP_UNLOCK(rm);
	RINGMAP_FUNC_DEBUG(end);
	return (err);
}


/* 
 * Do nothing here. This func will called when the last thread calls close(2).
 * But the callback: clear_capt_object() will be called every time by
 * close(2). Attention!!! - If the last thread call close(2) first will be
 * ringmap_close() called and then clear_capt_object() !!! 
 */
int
ringmap_close(struct cdev *cdev, int flag, int otyp, struct thread *td)
{
	RINGMAP_FUNC_DEBUG(The last capturing object is gone);
#if (__RINGMAP_DEB)
	printf(RINGMAP_PREFIX"[%s] pid = %d\n", __func__, td->td_proc->p_pid);
#endif 
    return (0);
}


/* 
 * Callback of ringmap_close() Free memory allocated for capturing object and
 * remove the capturing object from the list. 
 */
void 
clear_capt_object(void * data)
{
	struct capt_object *co = NULL;
	struct ringmap *rm = NULL;

	RINGMAP_FUNC_DEBUG(start);

	co = (struct capt_object *)data;

	RINGMAP_LOCK(co->rm);

	CAPT_OBJECT_DEB(co);

	rm = co->rm;
	contigfree(co->ring, sizeof(struct ring), M_DEVBUF);
	co->ring = NULL;

	SLIST_REMOVE(&rm->object_list, co, capt_object, objects);
	FREE(co, M_DEVBUF);

	if (rm->open_cnt > 0)
		--rm->open_cnt;
	 else 
		RINGMAP_WARN(Incorrect value of rm->open_cnt);
	RINGMAP_UNLOCK(rm);
	
	RINGMAP_FUNC_DEBUG(end);
}


/* doesn't work yet */
int 
ringmap_mmap_single(struct cdev *cdev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **object, int nprot)
{
	struct ringmap *rm = NULL;
	struct capt_object *co = NULL;
	vm_object_t obj;
	vm_map_entry_t  entry;
    vm_pindex_t pindex;
    vm_prot_t prot;
    boolean_t wired;

	rm = cdev2ringmap(cdev);
	if ( rm == NULL ) {
		RINGMAP_ERROR(Null pointer to ringmap structure);

		return (EIO);
	}

	SLIST_FOREACH(co, &rm->object_list, objects) {
		if (co->td == curthread) {
			break;
		}
	}

	if ((co == NULL) || (co->ring == NULL)) {
		RINGMAP_ERROR(Null pointer);
		return (EIO);
	}

	vm_map_lookup(&kmem_map, (vm_offset_t)co->ring, VM_PROT_ALL,
			      &entry, &obj, &pindex, &prot, &wired);
	vm_map_lookup_done(kmem_map, entry);

	if (obj == kmem_object) {
		RINGMAP_ERROR(Got kmem_object);
	} else {
		RINGMAP_FUNC_DEBUG(Got other obj);
	}

	object = &obj;

	return (0);
}


/*
 * Tells usre the physical addres of ring. User process will 
 * use this addres in order to map the buffer in its address 
 * space.
 */
int 
ringmap_read(struct cdev *cdev, struct uio *uio, int ioflag)
{
	struct ringmap *rm = NULL;
	struct capt_object *co = NULL;
	vm_paddr_t phys_ring_addr;
	int error;

	RINGMAP_FUNC_DEBUG(start);

	/* Get pointer to the ringmap structure */
	rm = cdev2ringmap(cdev);

	error = devfs_get_cdevpriv((void **)&co);
	if (error) {
		RINGMAP_ERROR(Can not access private data);
		return (error);
	} 

	CAPT_OBJECT_DEB(co);

	phys_ring_addr = vtophys(co->ring);
	uiomove(&phys_ring_addr, sizeof(phys_ring_addr), uio);

	RINGMAP_FUNC_DEBUG(end);

	return (0);
}


int
ringmap_ioctl (struct cdev *cdev, u_long cmd, caddr_t data, 
				int fflag, struct thread *td)
{
	int err = 0, err_sleep = err_sleep, size, flen, qn, i;

	struct ringmap *rm = NULL;
	struct capt_object *co;
	struct bpf_program *bpf_prog;
	struct bpf_insn *fcode;

	RINGMAP_IOCTL(start);

#if (RINGMAP_IOCTL_DEB)
	printf("[%s] pid = %d\n", __func__, td->td_proc->p_pid);
#endif 

	err = devfs_get_cdevpriv((void **)&co);
	if (err != 0) {
		RINGMAP_IOCTL(Error! Can not get private data!);
		return (err);
	}

	rm = co->rm;

	switch (cmd) {

		/* Sleep and wait for new packets */
		case IOCTL_SLEEP_WAIT:
			/* Set the new value into the adapter's TAIL register */
			rm->funcs->set_tail(co->ring->userrp, co->hw_rx_ring);

			CAPT_OBJECT_DEB(co);

			/* 
			 * Before we are going to sleep it makes a sence to check if we
			 * really must do it 
			 */
			while (RING_IS_EMPTY(co->ring)) {
				RINGMAP_IOCTL(Sleep and wait for new packets);

				/* Count how many times we wait for new packets */
				co->ring->user_wait_kern++;

				err = tsleep(co->ring, 
						(PRI_MAX_ITHD) | PCATCH, "ioctl", 0);
				/* go back into user-space by catching signal */
				if (err)
					goto out;
			}
		break;


		/* Synchronize sowftware ring-tail with hardware-ring-tail (RDT) */
		case IOCTL_SYNC_TAIL:
			RINGMAP_LOCK(rm);
			rm->funcs->set_tail(co->ring->userrp, co->hw_rx_ring);
			RINGMAP_UNLOCK(rm);
		break;


		case IOCTL_SETFILTER:
			bpf_prog = (struct bpf_program *)data;
			flen = bpf_prog->bf_len;
			if (flen > BPF_MAXINSNS) {
				RINGMAP_ERROR("bf_len > BPF_MAXINSNS");
				err = EINVAL;
				goto out;
			}
			size = flen * sizeof(*bpf_prog->bf_insns);
			fcode = (struct bpf_insn *)malloc(size, M_BPF, M_WAITOK);

			if (copyin((caddr_t)bpf_prog->bf_insns, (caddr_t)fcode, size) == 0 &&
						bpf_validate(fcode, (int)flen)) {
				co->fcode = (struct bpf_insn *)fcode;
			} else {
				RINGMAP_ERROR("Could not set filter");
				free((caddr_t)fcode, M_BPF);
				err = EINVAL;
				goto out;
			}
			/* 
			 * Everything went Ok. Set the filtering function 
			 * Think about hardware support for packet filtering!
			 */
			co->rm->funcs->pkt_filter = ringmap_bpf_filter;
		break;


		/* Tell user how many queues we have */
		case IOCTL_GETQUEUE_NUM:
			qn = rm->funcs->get_queuesnum();
			*(int *)data = qn;
		break;


		/* Associate the ring/queue with the capturing object */
		case IOCTL_ATTACH_RING:
			/* First stop receive and interupts while we allocate our data */
			rm->funcs->receive_disable(rm);
			rm->funcs->intr_disable(rm);

			qn = *(int *)data;
			/* Associate the capturing object with a queue */
			if (rm->funcs->set_queue(co, qn) == -1) {
				RINGMAP_ERROR(Queue attachment failed!);
				err = EINVAL;
				goto xxx;
			}
			/* Init ring-slots with mbufs and packets adrresses */
			for (i = 0 ; i < SLOTS_NUMBER ; i++) {
				if (set_slot(co, i) == -1) {
					RINGMAP_ERROR(Ring initialization failed!);
					err = EINVAL; 
					goto xxx;
				}
#if (__RINGMAP_DEB)
				PRINT_SLOT(co->ring, i);
#endif
			}
xxx:		
			rm->funcs->intr_enable(rm);
			rm->funcs->receive_enable(rm);
		break;
		

		default:
			RINGMAP_ERROR("Undefined command!");
			err = ENODEV;
	}
 
out: 
	RINGMAP_IOCTL(end);
	return (err);
}


int
set_slot(struct capt_object *co, unsigned int slot_num)
{
	struct ring *ring = co->ring;; 
	struct ringmap *rm = co->rm;

#if (__RINGMAP_DEB)
	printf("[%s] Set slot: %d\n", __func__, slot_num);
#endif

	ring->slot[slot_num].mbuf.kern = 
		(vm_offset_t)rm->funcs->get_mbuf(co->rx_buffers, slot_num);

	if (ring->slot[slot_num].mbuf.kern == 0)
		return (-1);

	ring->slot[slot_num].mbuf.phys = 
		(bus_addr_t)vtophys(ring->slot[slot_num].mbuf.kern);

	ring->slot[slot_num].packet.kern = 
		(vm_offset_t)rm->funcs->get_packet(co->rx_buffers, slot_num);
	ring->slot[slot_num].packet.phys = 
		(bus_addr_t)vtophys(ring->slot[slot_num].packet.kern);

	return (0);
} 
	

struct capt_object * 
ringmap_delayed_isr(void *context, struct ringmap *rm)
{
	struct capt_object *co = NULL;

	RINGMAP_INTR(start);

	RINGMAP_LOCK(rm);
	/* Do the next steps only if there is capturing process */ 
	if (rm->open_cnt > 0) {
		SLIST_FOREACH(co, &rm->object_list, objects) {
			if (co->intr_context == context) {
#if (RINGMAP_INTR_DEB)
				PRINT_RING_PTRS(co->ring);
#endif
				rm->funcs->set_tail(co->ring->userrp, co->hw_rx_ring);
				/* set hardware speciffic time stamp function */
				getmicrotime(&co->ring->last_ts);
				++co->ring->intr_num;
				break;
			}
		}
	}
	RINGMAP_UNLOCK(rm);

	RINGMAP_INTR(end);

	return (co);
}


void 
per_packet_iteration(struct capt_object *co, int slot_num)
{
	struct ringmap *rm = co->rm;

	RINGMAP_INTR(start);

	co->ring->slot[slot_num].is_ok = 1;
	co->ring->slot[slot_num].intr_num = co->ring->intr_num;
	
	if (rm->funcs->pkt_filter != NULL)
		rm->funcs->pkt_filter(co, slot_num);

	co->ring->kernrp = slot_num;
#ifdef RINGMAP_TIMESTAMP
	co->ring->slot[slot_num].ts = co->ring->last_ts;
#endif

	PRINT_SLOT_DEB(co->ring, slot_num);

	RINGMAP_INTR(end);
}


/* Paket filtering: wrapper around bpf_filter() */ 
void
ringmap_bpf_filter(struct capt_object *co, int slot_num)
{
	struct mbuf *mb = (struct mbuf *)K_MBUF(co->ring, slot_num);
	unsigned int pktlen = mb->m_len, slen;

	if (co->fcode != NULL) {
		slen = bpf_filter(co->fcode, (u_char *)mb, pktlen, 0);
		co->ring->slot[slot_num].filtered = (slen + 1) ? slen:0;
	}
}


void 
ringmap_timestamp(struct ring_slot *slot, struct timeval *ts)
{
	if (ts == NULL) 
		getmicrotime(&slot->ts);
	else 
		slot->ts = *(ts);
}


struct ringmap *
dev2ringmap(device_t dev)
{
	struct ringmap *rm = NULL;

	SLIST_FOREACH(rm, &ringmap_list_head, entries) {
		if (rm->dev == dev) 
			return (rm);
	}
	return (rm);
}


struct ringmap *
cdev2ringmap(struct cdev *cdev)
{
	struct ringmap *rm = NULL;
	
	SLIST_FOREACH(rm, &ringmap_list_head, entries) {
		if (rm->cdev == cdev) 
			return (rm);
	}
	return (rm);
}
