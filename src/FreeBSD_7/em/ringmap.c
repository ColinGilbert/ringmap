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

#include <machine/bus.h>
#include <machine/atomic.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>


#ifdef __E1000_RINGMAP__
#include "e1000_api.h"
#include "if_em.h"

extern devclass_t em_devclass;
extern int	em_rxeof(struct adapter *, int);
extern void	em_print_debug_info(struct adapter *);
#endif

#include "ringmap.h"

/* DON'T TOUCH IT */
int fiveg_da_2009 = 1;

/* F U N C T I O N S */
int ringmap_attach(struct adapter *);
int ringmap_detach(struct adapter*);
struct adapter* get_adapter_struct(struct cdev *dev);
int ringmap_print_ring_pointers(struct adapter *);
void ringmap_print_ring (struct adapter *adapter, int level);
void ringmap_print_slot(struct adapter *adapter, unsigned int slot_number);
void ringmap_handle_rxtx(void *context, int pending);

d_open_t	ringmap_open;
d_close_t	ringmap_close;
d_ioctl_t	ringmap_ioctl;
d_mmap_t	ringmap_mmap;

/*
 *	Character Device for access on if_em driver structures
 */
static struct cdevsw ringmap_devsw = {
	/*	version */	.d_version 	= D_VERSION,
	/* 	open 	*/	.d_open 	= ringmap_open,
	/* 	close 	*/	.d_close 	= ringmap_close,
	/*	ioctl	*/	.d_ioctl	= ringmap_ioctl,
	/*	mmap	*/	.d_mmap		= ringmap_mmap,
	/* 	name 	*/	.d_name 	= "ringmap_cdev"
};

/*
 * Will called from if_em.c before returning from 
 * em_attach() function.  
 */
int ringmap_attach(struct adapter *a) {	
	struct adapter *adapter = a; 
	struct ringmap *rm;
	struct ring *ring;

	RINGMAP_FUNC_DEBUG(begin);

	/* Disable interrupts while we set our structures */
	RINGMAP_HW_DISABLE_INTR(adapter);

	/* Alloc mem for ringmap structure */
	// MALLOC(rm, struct ringmap *, sizeof(struct ringmap), M_DEVBUF,
	//		M_WAITOK|M_ZERO); 
	rm = (struct ringmap *) contigmalloc (sizeof(struct ringmap), 
			M_DEVBUF, M_ZERO, 0, -1L, PAGE_SIZE, 0);

	if (rm == NULL) { 
		RINGMAP_ERROR(Can not allocate space for ringmap structure); 
		return (-1); 
	}
	if ((vm_offset_t)(rm) & PAGE_MASK){
		RINGMAP_ERROR(rm is not allined to PAGE_MASK);
		return (-1);
	}

	/*Alloc mem for ring structure */
	// MALLOC(ring, struct ring *, sizeof(struct ring), M_DEVBUF,
	//		M_WAITOK|M_ZERO); 
	

	ring = (struct ring *) contigmalloc (sizeof(struct ring), 
			M_DEVBUF, M_ZERO, 0, -1L, PAGE_SIZE, 0);
	if (ring == NULL) { 
		RINGMAP_ERROR(Can not allocate space for ringmap structure); 
		return (-1); 
	}
	if ((vm_offset_t)(ring) & PAGE_MASK){
		RINGMAP_ERROR(rm is not allined to PAGE_MASK);
		return (-1);
	}

	rm->ring = ring;

	/* 
	 * Create char device for communication with user space. User space process
	 * wich want to capture should first open this device. Then, by syscalls 
	 * on this device it will: 
	 * - 	get physical adresses of packet buffers for mapping them in its 
	 * 		virtual memory space 
	 *
	 * -	controll packet capturing: start, stop, sleep to wait for packets.
	 */
	rm->ringmap_dev = make_dev(&ringmap_devsw, device_get_unit(adapter->dev),
			UID_ROOT, GID_WHEEL, 0666, RINGMAP_DEVICE"%s",
			device_get_nameunit(adapter->dev));

	/* Counts how many times the device will opened */
	rm->open_cnt = 0;

	/* Pointer to structure of process wich opened the device */	
	rm->procp = NULL;

	adapter->rm = rm; 
	rm->adapter = adapter;

	RINGMAP_FUNC_DEBUG(end); 

	return (0);	
}


int
ringmap_detach(struct adapter *adapter)
{
	struct ringmap *rm;

	RINGMAP_FUNC_DEBUG(start);
	
	if (adapter == NULL){
		RINGMAP_ERROR(NUll pointer to adapter structure);
		return (1);
	}

	rm = adapter->rm;
	
	/* Disable pkts receive and interrupts while we set our structures */
	RINGMAP_HW_DISABLE_INTR(adapter);
	RINGMAP_HW_DISABLE_RECEIVE(adapter);

	/* May be any tasks in queue */
	taskqueue_free(adapter->tq);
	
	destroy_dev(rm->ringmap_dev);

	// FREE(rm, M_DEVBUF);
	
	RINGMAP_FUNC_DEBUG(end);

	return (0);
}


/******************************************************************
 * Open device and get the pointer of user process structure!!! 
 * We will use the address space of this process to map there 
 * the mbufs and buffers with packet data. So it all will 
 * be placed and  accesseble in this user proccess.
 ******************************************************************/
int
ringmap_open(struct cdev *dev, int flag, int otyp, struct thread *td)
{
	unsigned int i; 
	struct ring_slot; 
	struct adapter *adapter = (struct adapter *)get_adapter_struct(dev);
	struct ringmap *rm = adapter->rm;

	RINGMAP_FUNC_DEBUG(start);

#if (__RINGMAP_DEB) 
	printf("[%s]: dev_t=%d, flag=%x, otyp=%x, iface=%s\n", __func__,
	      dev2udev(dev), flag, otyp, device_get_nameunit(adapter->dev));
#endif 

	/**
	 **	Only one process only one time can open our device !!!
	 **/
	if (!atomic_cmpset_int(&rm->open_cnt, 0, 1)){
		RINGMAP_ERROR(Sorry! Device is opened!);
		return (ENODEV);
	}
	
	/* Disable interrupts of adapter */
	RINGMAP_HW_DISABLE_INTR(adapter);

	/* Disable Flow Control */
	RINGMAP_HW_DISABLE_FLOWCONTR(adapter);

	/*
	 * Prepare ring for caputure 
	 */
    rm->procp = (struct proc *)td->td_proc;
	rm->td = td;
	RINGMAP_INIT(rm->ring, adapter);

	for (i = 0 ; i < SLOTS_NUMBER ; i ++) {
		if (rm->adapter->rx_buffer_area[i].m_head == NULL) {
			printf(ERR_PREFIX"[%s] mbuf for descriptor=%d is not allocated\n", __func__, i);
			printf(ERR_PREFIX"[%s] The reason may be: ifnet structure for our network device not present or not initialized\n", __func__);
			return (EFAULT);
		}

		rm->adapter->rx_desc_base[i].status = 0;

		rm->ring->slot[i].mbuf.kern = (vm_offset_t) RINGMAP_GET_MBUF_P(rm->adapter, i);
		rm->ring->slot[i].mbuf.phys = (bus_addr_t) vtophys(RINGMAP_GET_MBUF_P(rm->adapter, i));

		rm->ring->slot[i].packet.kern = (vm_offset_t) RINGMAP_GET_PACKET_P(rm->adapter, i);
		rm->ring->slot[i].packet.phys = (bus_addr_t)	vtophys(RINGMAP_GET_PACKET_P(rm->adapter, i));

		rm->ring->slot[i].descriptor.kern = (vm_offset_t) RINGMAP_GET_DESCRIPTOR_P(rm->adapter, i);
		rm->ring->slot[i].descriptor.phys = (bus_addr_t)	vtophys(RINGMAP_GET_DESCRIPTOR_P(rm->adapter, i));

#if (__RINGMAP_DEB)
		ringmap_print_slot(adapter, i);
#endif
	}
	rm->ring->hw_stats.kern = (vm_offset_t)(&adapter->stats);
	rm->ring->hw_stats.phys = (bus_addr_t)vtophys(&adapter->stats);


	RINGMAP_HW_ENABLE_INTR(adapter);

	RINGMAP_FUNC_DEBUG(end);

	return (0);
}


int
ringmap_close(struct cdev *dev, int flag, int otyp, struct thread *td)
{
	struct adapter *adapter = (struct adapter *)get_adapter_struct(dev);
	struct ringmap *rm = adapter->rm;

	RINGMAP_FUNC_DEBUG(start);
	
	/* Disable interrupts while we set our structures */
	RINGMAP_HW_DISABLE_INTR(adapter);

#if (__RINGMAP_DEB) 
	printf("[%s]: dev_t=%d, flag=%x, otyp=%x, iface=%s\n", __func__,
	      dev2udev(dev), flag, otyp, device_get_nameunit(adapter->dev));
#endif 

	/* After close there is no capturing process */
	rm->procp = NULL;
	rm->td = NULL;

	atomic_readandclear_int(&rm->open_cnt); 

	RINGMAP_FUNC_DEBUG(end);
    return (0);
}


int
ringmap_mmap(struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{
	struct adapter *adapter = (struct adapter *)get_adapter_struct(dev);
	struct ringmap *rm = adapter->rm;

	RINGMAP_FUNC_DEBUG(start);

	if (nprot & PROT_EXEC) {
		RINGMAP_WARN("PROT_EXEC ist set");
		return (ERESTART);
	}

	/* We want to map ring in user-space. offset is not needed! */
	offset = 0;
	*paddr = vtophys((rm->ring) + offset);

	RINGMAP_FUNC_DEBUG(end);

    return(0);
}


int
ringmap_ioctl (struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int err = 0, err_sleep;
	struct adapter *adapter = (struct adapter *)get_adapter_struct(dev);
	struct ringmap *rm = adapter->rm;

	unsigned int *userp = NULL;

	RINGMAP_FUNC_DEBUG(start);

	switch( cmd ){

		/* Tell to user number of descriptors */
		case IOCTL_G_DNUM:
			RINGMAP_OUTPUT(IOCTL_G_DNUM);
			
			userp = (unsigned int *)(*(unsigned int *)data);
			if (userp == NULL){
				RINGMAP_ERROR(NULL pointer by ioctl IOCTL_G_DNUM);
				return (EINVAL);
			}

			unsigned int dn = (unsigned int)adapter->num_rx_desc;
			copyout(&dn, userp, sizeof(unsigned int));

		break; 

		/* Enable Receive and Interrupts */
		case IOCTL_ENABLE_RECEIVE:
			RINGMAP_IOCTL(IOCTL_ENABLE_RECEIVE);
			RINGMAP_HW_ENABLE_INTR(adapter);
			RINGMAP_HW_ENABLE_RECEIVE(adapter);
		break;

		/* Disable Receive and Interrupts */
		case IOCTL_DISABLE_RECEIVE:
			RINGMAP_IOCTL(IOCTL_DISABLE_RECEIVE);
			RINGMAP_HW_DISABLE_INTR(adapter);
			RINGMAP_HW_DISABLE_RECEIVE(adapter);
		break;
		
		/* Disable Flow Control */
		case IOCTL_DISABLE_FLOWCNTR:
			RINGMAP_IOCTL(IOCTL_DISABLE_FLOWCNTR);
			RINGMAP_HW_DISABLE_FLOWCONTR(adapter);
		break;

		/* Sleep and wait for new frames */
		case IOCTL_SLEEP_WAIT:
			rm->ring->user_wait_kern++;
			RINGMAP_HW_SYNC_TAIL(adapter);
			err_sleep = tsleep(rm, (PRI_MIN) | PCATCH, "ioctl", hz);
		break;

		/* Synchronize sowftware ring-tail with hardware-ring-tail (RDT) */
		case IOCTL_SET_RDT:
			SET_RDT(adapter);
		break;

		default:
			RINGMAP_ERROR("Undefined command!");
			return (ENODEV);
	}   	
        	
	RINGMAP_FUNC_DEBUG(end);
        	
	return (err);
}

void
ringmap_handle_rxtx(void *context, int pending)
{
	struct adapter	*adapter = context;
	struct ifnet	*ifp = adapter->ifp;
	struct ringmap 	*rm = adapter->rm;

#if (INTR_DEB) 	
	printf("########################################################################\n");
#endif

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
#ifdef __E1000_RINGMAP__
		if (em_rxeof(adapter, adapter->rx_process_limit) != 0)
			taskqueue_enqueue(adapter->tq, &adapter->rxtx_task);
#endif
	}

#if (INTR_DEB) 	
	printf("########################################################################\n\n");
#endif

	RINGMAP_HW_ENABLE_INTR(adapter);

	if (rm->procp != NULL) {
		wakeup(rm);
	}
}


struct adapter*
get_adapter_struct(struct cdev *dev)
{
	struct adapter *adapter;

	adapter = RINGMAP_GET_ADAPTER_STRUCT(adapter);
	return (adapter);
}


void
ringmap_print_ring (struct adapter *adapter, int level)
{
	struct ringmap *rm = adapter->rm;

	printf("Ring Size = %d \n",rm->ring->size );	
	printf("Times Kern wait for User = %llu \n",rm->ring->kern_wait_user);	
	printf("Times User wait for Kern = %llu \n",rm->ring->user_wait_kern);	
	printf("Interrupts Counter = %llu \n",rm->ring->interrupts_counter);	

	ringmap_print_ring_pointers(adapter);
}


void 
ringmap_print_slot(struct adapter *adapter, unsigned int slot_number)
{
	struct ringmap *rm = adapter->rm;

	printf("Slot Number: %d \n", slot_number);
	printf("---------------- \n");

	printf("[%s] physical addr of descriptor[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring->slot[slot_number].descriptor.phys);
	printf("[%s] kernel addr of descriptor[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring->slot[slot_number].descriptor.kern);
	printf("[%s] physical addr of mbuf[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring->slot[slot_number].mbuf.phys);
	printf("[%s] kernel addr of mbuf[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring->slot[slot_number].mbuf.kern);
	printf("[%s] physical addr of packet_buffer[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring->slot[slot_number].packet.phys);
	printf("[%s] kernel addr of packet_buffer[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring->slot[slot_number].packet.kern);
	printf(" \n");
}


int 
ringmap_print_ring_pointers(struct adapter *adapter)
{
	unsigned int rdt, rdh;
	struct ringmap *rm = adapter->rm;

	rdh = RINGMAP_HW_READ_HEAD(adapter);
	rdt = RINGMAP_HW_READ_TAIL(adapter);

	printf("\n  +++++++++  RING POINTERS  ++++++++++++ \n");
	printf("  +  RDH = %d (KERN POINTER)\n", rdh);
	printf("  +  RDT = %d (USER POINTER)\n", rdt);
	printf("  +\n");
	printf("  +  kernrp = %d \n", rm->ring->kernrp);
	printf("  +  userrp = %d \n", rm->ring->userrp);
	printf("  ++++++++++++++++++++++++++++++++++++++ \n\n");

	return (0);
}
