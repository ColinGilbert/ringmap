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
int ringmap_detach (device_t);
void ringmap_close_cb (void *data);
void clear_capt_object(void *);
void print_capt_obj(struct capt_object *);
struct ringmap * cdev2ringmap(struct cdev *);
struct ringmap * dev2ringmap(device_t);
void ringmap_bpf_filter(struct capt_object *, int);
void per_packet_iteration(struct ringmap *, int );
void ringmap_delayed_isr(void *context, struct ringmap *rm);

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
 * Will called from if_em.c before returning from 
 * em_attach() function.  
 */
struct ringmap *
ringmap_attach(device_t dev, struct ringmap_functions *rf) 
{	
	struct ringmap *rm = NULL;

	RINGMAP_FUNC_DEBUG(begin);

	MALLOC(rm, struct ringmap *, sizeof(struct ringmap), 
			M_DEVBUF, (M_ZERO | M_WAITOK));

	if (rm == NULL) { 
		RINGMAP_ERROR(Can not allocate space for ringmap structure);
		return (NULL);
	}

	/* 
	 * Create char device for communication with user-space. The user-space
	 * process wich want to capture packets should first open this device.
	 * Then, by syscalls on this device it will: 
	 * - 	get physical adresses of packet buffers for mapping them in its 
	 * 		virtual memory space 
	 *
	 * -	controll packet capturing: start, stop, sleep to wait for packets.
	 */
	rm->cdev = make_dev(&ringmap_devsw, device_get_unit(dev),
						UID_ROOT, GID_WHEEL, 0666, 
						device_get_nameunit(dev));
	if (rm->cdev == NULL) {
		RINGMAP_ERROR(Can not create char device);
		FREE(rm, M_DEVBUF);
		return (NULL);
	}

	/* 
	 * Tell to ringmap which hardware and driver speciffic functions 
	 * should it use 
	 */
	rm->funcs = rf;

	/* Store adapters device structure in ringmap */
	rm->dev = dev;

	/* 
	 * Initialize the list of capturing objects. Each object will represent the
	 * thread that capture traffic and its ring.
	 */
	SLIST_INIT(&rm->object_list);

	/* Insert ringmap structure into the list */
	SLIST_INSERT_HEAD(&ringmap_list_head, rm, entries);

	/* Init the mutex to protecting our data */
	RINGMAP_LOCK_INIT(rm, device_get_nameunit(dev));

	/* 
	 * Set default function if the hardware specific functions are not set
	 */
	if (rm->funcs->delayed_isr != NULL)
		rm->funcs->delayed_isr = ringmap_delayed_isr;

	if (rm->funcs->per_packet_iteration != NULL)
		rm->funcs->per_packet_iteration = per_packet_iteration;


	RINGMAP_FUNC_DEBUG(end); 

	/* 
	 * Return ringmap pointer to the generic driver. Generic driver should
	 * store the pointer in the adapter structure in order to be able to access
	 * ringmap
	 */
	return (rm);	
}


/* 
 * Should be called from driver's detach function. It is a little dangerous 
 * place - probably we shoul protect our code here with locks!!!
 */
int
ringmap_detach(device_t dev)
{
	struct ringmap *rm = NULL;
	struct capt_object *co = NULL;

	RINGMAP_FUNC_DEBUG(start);
	
	rm = dev2ringmap(dev);
	if (rm == NULL) {
		RINGMAP_WARN(Can not get pointer to ringmap structure);
		return (-1);
	}
	
	/* Remove all capturing objects associated with ringmap */
    while (!SLIST_EMPTY(&rm->object_list)) {
	    co = SLIST_FIRST(&rm->object_list);
	    clear_capt_object((void *)co);
    }

	/* Destroy char device associated with ringmap */
	if (rm->cdev != NULL)
		destroy_dev(rm->cdev);

	RINGMAP_LOCK_DESTROY(rm);

	/* And remove ringmap from global list */
	SLIST_REMOVE(&ringmap_list_head, rm, ringmap, entries);
	
	FREE(rm, M_DEVBUF);

	RINGMAP_FUNC_DEBUG(end);

	return (0);
}


/******************************************************************
 * This func will called as result of open(2). Here we allocate 
 * the memory for the new ring and capt_object structure (so called 
 * capturing object). Capturing object represents the thread with 
 * its ring.
 ******************************************************************/
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

	/* 
	 * create the capturing object wich will represent 
	 * current thread and packets ring 
	 */
	MALLOC(co, struct capt_object *, 
			sizeof(struct capt_object), M_DEVBUF, (M_ZERO | M_WAITOK));
	if ( co == NULL ){
		contigfree(ring, sizeof(struct ring), M_DEVBUF);
		err = EIO; goto out;
	}

	ring->size = SLOTS_NUMBER;
	ring->pid = td->td_proc->p_pid;

	co->ring = ring;
	co->td = td;
	co->rm = rm;

	/* The next should be probably done in the ioctl() */
#ifdef DEFAULT_QUEUE 
	/* Associate the capturing object with a queue */
	if (rm->funcs->set_queue(co, DEFAULT_QUEUE) == -1) {
		RINGMAP_ERROR(Can not associate que with the capturing object!);

		contigfree(ring, sizeof(struct ring), M_DEVBUF);
		FREE(co, M_DEVBUF);

		err = EIO; goto out;
	}
#endif 

	/* Init ring-slots with mbufs and packets adrresses */
	for (i = 0 ; i < SLOTS_NUMBER ; i++) {
		if (rm->funcs->set_slot(co, i) == -1){
			RINGMAP_ERROR(Ring initialization failed!);

			contigfree(ring, sizeof(struct ring), M_DEVBUF);
			FREE(co, M_DEVBUF);

			err = EIO; goto out;
		}
#if (__RINGMAP_DEB)
		PRINT_SLOT(ring, i);
#endif
	}

	/* 
	 * Insert the capturing object in the single linked list
	 * the head of the list is in the ringmap structure
	 */
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

#if (__RINGMAP_DEB)
	print_capt_obj(co);
	PRINT_RING_PTRS(co->ring); 
#endif

out:
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
 * Callback of ringmap_close() 
 * Free memory allocated for capturing object and remove the 
 * capturing object from the list. 
 */
void 
clear_capt_object(void * data)
{
	struct capt_object *co = NULL;
	struct ringmap *rm = NULL;

	RINGMAP_FUNC_DEBUG(start);

	if (data != NULL) {
		co = (struct capt_object *)data;

		RINGMAP_LOCK(co->rm);

		/* to be completely sure */
		if (co == NULL) 
			goto out;

		rm = co->rm;
#if (__RINGMAP_DEB)
		printf("[%s] Object to delete:\n", __func__);
		print_capt_obj(co);
#endif 
		if (co->ring != NULL)
			contigfree(co->ring, sizeof(struct ring), M_DEVBUF);

		SLIST_REMOVE(&rm->object_list, co, capt_object, objects);
		FREE(co, M_DEVBUF);
		data = co = NULL;

		if (rm->open_cnt) {
			--rm->open_cnt;
		} else {
			RINGMAP_WARN(Incorrect value of rm->open_cnt);
		}
out: 
		RINGMAP_UNLOCK(rm);

	} else {
		RINGMAP_FUNC_DEBUG(NULL pointer to the capturing object!);
	}
	
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


	RINGMAP_FUNC_DEBUG(start);


	rm = cdev2ringmap(cdev);
	if ( rm == NULL ) {
		RINGMAP_ERROR(Null pointer to ringmap structure);

		return (EIO);
	}

	SLIST_FOREACH(co, &rm->object_list, objects) {
		if (co->td == curthread){
			break;
		}
	}

	if ((co == NULL) || (co->ring == NULL)){
		RINGMAP_ERROR(Null pointer);
		return (EIO);
	}

	vm_map_lookup(&kmem_map, (vm_offset_t)co->ring, VM_PROT_ALL,
			      &entry, &obj, &pindex, &prot, &wired);
	vm_map_lookup_done(kmem_map, entry);

	if (obj == kmem_object){
		RINGMAP_ERROR(Got kmem_object);
	} else {
		RINGMAP_FUNC_DEBUG(Got other obj);
	}

	object = &obj;

	RINGMAP_FUNC_DEBUG(start);

	return (0);
}


void 
per_packet_iteration(struct ringmap *rm, int slot_num)
{
	struct capt_object *co = NULL;

	RINGMAP_INTR(start);

	SLIST_FOREACH(co, &rm->object_list, objects) {
		if (co->ring != NULL) {
			co->ring->slot[slot_num].is_ok = 1;
			co->ring->slot[slot_num].intr_num = co->ring->intr_num;
			
			if (rm->funcs->pkt_filter != NULL)
				rm->funcs->pkt_filter(co, slot_num);

#ifdef RINGMAP_TIMESTAMP
			co->ring->slot[slot_num].ts = co->ring->last_ts;
#endif
#if (RINGMAP_INTR_DEB)
			PRINT_SLOT(co->ring, slot_num);
#endif
		}
	}

	RINGMAP_INTR(end);
}


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
		return(error);
	} 

	if (co->td != curthread ){
		RINGMAP_ERROR(Wrong capturing object!);
		return(EIO);
	}

	phys_ring_addr = vtophys(co->ring);

#if (__RINGMAP_DEB)
	print_capt_obj(co);
	PRINT_RING_PTRS(co->ring);
#endif

	uiomove(&phys_ring_addr, sizeof(phys_ring_addr), uio);

	RINGMAP_FUNC_DEBUG(end);

	return (0);
}


int
ringmap_ioctl (struct cdev *cdev, u_long cmd, caddr_t data, 
				int fflag, struct thread *td)
{
	int err = 0, err_sleep = err_sleep, size, flen;

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

			/* Count how many times we wait for new packets */
			co->ring->user_wait_kern++;

			/* Set adapter's TAIL register */
			rm->funcs->sync_tail(co);

#if (RINGMAP_IOCTL_DEB)
			print_capt_obj(co);
			PRINT_RING_PTRS(co->ring);
#endif
			/* 
			 * In the time: from user has called ioctl() until now could 
			 * come the new packets. It means, before we are going to sleep
			 * it makes a sence to check if we really must do it 
			 */
			while (RING_IS_EMPTY(co->ring)) {
				RINGMAP_IOCTL(Sleep and wait for new packets);

				err = tsleep(co->ring, 
						(PRI_MAX_ITHD) | PCATCH, "ioctl", 0);

				/* go back in user-space by catching signal */
				if (err)
					goto out;
			}
		break;

		/* Synchronize sowftware ring-tail with hardware-ring-tail (RDT) */
		case IOCTL_SYNC_TAIL:
			RINGMAP_LOCK(rm);
			rm->funcs->sync_tail(co);
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

		default:
			RINGMAP_ERROR("Undefined command!");
			err = ENODEV;
	}
 
out: 

	RINGMAP_IOCTL(end);
 
	return (err);
}

	
void 
ringmap_delayed_isr(void *context, struct ringmap *rm)
{
	struct capt_object *co = NULL;
	struct timeval	last_ts;

	RINGMAP_INTR(start);

	RINGMAP_LOCK(rm);
	/* Do the next steps only if there is capturing process */ 
	if (rm->open_cnt > 0) {

		/* TODO: do it through our set_timestamp() */
		getmicrotime(&last_ts);

		SLIST_FOREACH(co, &rm->object_list, objects) {
			if (co->intr_context == context) {
#if (RINGMAP_INTR_DEB)
				PRINT_RING_PTRS(co->ring);
#endif
				rm->funcs->sync_tail(co);
				co->ring->last_ts = last_ts;
				++co->ring->intr_num;
			}
		}
	}
	RINGMAP_UNLOCK(rm);

	RINGMAP_INTR(end);
}


/* Paket filtering: wrapper about bpf_filter() */ 
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
print_capt_obj(struct capt_object *co)
{
	if (co != NULL) {
		printf("\n===  co->td->proc->pid: %d\n", 
				co->td->td_proc->p_pid);

		printf("===  Ring Kernel Addr:0x%X\n", 
				(unsigned int)co->ring);

		/* Print addr of que only if multiqueue supported */
		if (co->que != NULL)
			printf("===  Queue Kernel Addr:0x%X\n\n", 
					(unsigned int)co->que);
	} else {
		RINGMAP_WARN(NULL pointer: capturing object);
	}
}


struct ringmap *
dev2ringmap(device_t dev)
{
	struct ringmap *rm = NULL;

	SLIST_FOREACH(rm, &ringmap_list_head, entries) {
		if (rm->dev == dev) 
			return(rm);
	}
	return(rm);
}


struct ringmap *
cdev2ringmap(struct cdev *cdev)
{
	struct ringmap *rm = NULL;
	
	SLIST_FOREACH(rm, &ringmap_list_head, entries) {
		if (rm->cdev == cdev) 
			return(rm);
	}
	return(rm);
}
