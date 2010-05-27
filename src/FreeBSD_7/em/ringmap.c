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

#include "ringmap.h"

#include "e1000_api.h"
#include "if_em.h"

/* V A R S   A N D   P O I N T E R S */
extern devclass_t em_devclass;
extern int	em_rxeof(struct adapter *, int);
extern void	em_print_debug_info(struct adapter *);

/* DON'T TOUCH IT */
int fiveg_da_2009 = 1;

/* F U N C T I O N S */
int check_pointers(struct adapter *);
int ringmap_attach(struct adapter *);
int ringmap_detach(struct adapter*);
struct adapter* get_adapter_struct(struct cdev *dev);
void ringmap_wdog(void *);
int ringmap_disable_receive(struct adapter *);
int ringmap_enable_receive(struct adapter *adapter);
int ringmap_enable_intr(struct adapter *);
int ringmap_disable_intr(struct adapter *);
void ringmap_disable_flowcontr(struct adapter *);
int ringmap_print_ring_pointers(struct adapter *);
void ringmap_print_ring (struct adapter *adapter, int level);
void ringmap_print_slot(struct adapter *adapter, unsigned int slot_number);
void ringmap_handle_rxtx(void *context, int pending);

d_open_t	ringmap_open;
d_close_t	ringmap_close;
d_read_t	ringmap_read;
d_ioctl_t	ringmap_ioctl;

/*
 *	Character Device for access on if_em driver structures
 */
static struct cdevsw ringmap_devsw = {
	/*	version */	.d_version 	= D_VERSION,
	/* 	open 	*/	.d_open 	= ringmap_open,
	/* 	close 	*/	.d_close 	= ringmap_close,
	/* 	read 	*/	.d_read 	= ringmap_read,
	/*	ioctl	*/	.d_ioctl	= ringmap_ioctl,
	/* 	name 	*/	.d_name 	= "ringmap_cdev"
};

/*
 * Will called from if_em.c before returning from 
 * em_attach() function.  
 */
int ringmap_attach(struct adapter *a) {	struct adapter *adapter = a; struct
	ringmap *rm;

	RINGMAP_FUNC_DEBUG(begin);

	/* Disable pkts receive and interrupts while we set our structures */
	ringmap_disable_intr(adapter);

	if (check_pointers(adapter)){ return (-1); }

	/* Alloc mem for ringmap structure */
	MALLOC(rm, struct ringmap *, sizeof(struct ringmap), M_DEVBUF,
			M_WAITOK|M_ZERO); 
	
	if (rm == NULL){ RINGMAP_ERROR(Can not allocate
				space for ringmap structure); return (-1); }

	rm->ringmap_dev = make_dev(&ringmap_devsw, device_get_unit(adapter->dev),
			UID_ROOT, GID_WHEEL, 0666, RINGMAP_DEVICE"%s",
			device_get_nameunit(adapter->dev));


	/* Init of timer that have to be start to wait for user space capturing
	 * process */
	callout_init(&rm->ring_callout, 1);

	rm->open_cnt = 0; rm->procp = NULL;

	adapter->rm = rm; rm->adapter = adapter;

	RINGMAP_FUNC_DEBUG("end"); return (0);	}


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
	ringmap_disable_intr(adapter);
	ringmap_disable_receive(adapter);

	/* May be any tasks in queue */
	taskqueue_free(adapter->tq);
	
	callout_stop(&rm->ring_callout);

	destroy_dev(rm->ringmap_dev);

	FREE(rm, M_DEVBUF);
	
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

#if (__RINGMAP_DEB) 
	printf("[%s]: dev_t=%d, flag=%x, otyp=%x, iface=%s\n", __func__,
	      dev2udev(dev), flag, otyp, device_get_nameunit(adapter->dev));
#endif 

	/**
	 **	Only one process only one time can open device !!!
	 **/
	if (!atomic_cmpset_int(&rm->open_cnt, 0, 1)){
		RINGMAP_ERROR(Sorry! Device is opened!);
		return (-EIO);
	}
	
	/*
	 * NULL pointers check
	 */
	if (check_pointers(adapter)) {
		atomic_readandclear_int(&rm->open_cnt);
		return (-EIO);
	}

	ringmap_disable_intr(adapter);

	/* Disable Flow Control */
	ringmap_disable_flowcontr(adapter);

	rm->times_restart_callout 	= 0;
    rm->procp = (struct proc *)td->td_proc;
	rm->td = td;

	INIT_RING_AND_REGISTERS(&rm->ring, adapter);

	for (i = 0 ; i < SLOTS_NUMBER ; i ++)
			rm->adapter->rx_desc_base[i].status = 0;

	ringmap_enable_intr(adapter);

	RINGMAP_OUTPUT("end");
	return (0);
}


int
ringmap_close(struct cdev *dev, int flag, int otyp, struct thread *td)
{
	struct adapter *adapter = (struct adapter *)get_adapter_struct(dev);
	struct ringmap *rm = adapter->rm;
	RINGMAP_OUTPUT("begin");
	
	/* Disable pkts receive and interrupts while we set our structures */
	ringmap_disable_intr(rm->adapter);

#if (__RINGMAP_DEB) 
	printf("[%s]: dev_t=%d, flag=%x, otyp=%x, iface=%s\n", __func__,
	      dev2udev(dev), flag, otyp, device_get_nameunit(adapter->dev));
#endif 

	/* After close there is no capturing process */
	rm->times_restart_callout 	= 0;
	rm->procp = NULL;
	rm->td = NULL;

	if (atomic_readandclear_int(&rm->open_cnt) > 1){
		RINGMAP_WARN(More than one processes has opened the device);
	}

	em_print_debug_info(adapter);

	RINGMAP_OUTPUT("end");
    return (0);
}


int 
check_pointers(struct adapter *adapter)
{
	
	if (adapter == NULL){
		RINGMAP_ERROR("adapter = NULL. We don't have a pointer to adapter structure");
		return (-1);
	}

	if (adapter->rx_buffer_area == NULL) {
		RINGMAP_ERROR(rx_buffer_area is not allocated! Driver structures is not initialized);
		RINGMAP_ERROR(INFO: rx_buffer_area - pointer to buffer with em_buffer structures);
		RINGMAP_ERROR(INFO: em_buffer - contains the pointer (m_head) to mbuf);
		RINGMAP_ERROR(INFO: All of these things should be allocated before we begin with capturing);
		return (-1);
	}
	
	if (adapter->rx_desc_base == NULL) {
		RINGMAP_ERROR(rx_desc_base is not allocated! Driver structures is not initialized);
		RINGMAP_ERROR(INFO: rx_desc_base - pointer to buffer with descriptor structures);
		return (-1);
	}

	if (adapter->num_rx_desc == 0){
		RINGMAP_ERROR(Number of descriptors = 0);
		return (-1);
	}

	if (adapter->ifp == NULL){
		RINGMAP_ERROR(ifnet structure is not allocated);
		return (-1);
	}
	if (&adapter->stats == NULL){
		RINGMAP_ERROR(NIC statistics structure is not alloced);
		return (-1);
	}

	return (0);
}


int
ringmap_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	RINGMAP_FUNC_DEBUG(begin);
    int err = EIO, i;
	struct adapter *adapter = (struct adapter *)get_adapter_struct(dev);
	struct ringmap *rm = adapter->rm;

	/* Physical addres of ring and nic_statistics structures */
	bus_addr_t nic_statspp, rspp; 

#if (__RINGMAP_DEB) 
    printf("[%s] dev_t=%d, uio=%p, ioflag=%d\n",
			__func__, dev2udev(dev), uio, ioflag);
#endif 

	/* Check important pointers and values: if NULL, 0 , etc */
	if (check_pointers(adapter)) {
		return (-err);
	}

	if (!(adapter->ifp->if_flags & IFF_DRV_RUNNING)) {
		RINGMAP_WARN("ifnet interface is not running!");
	}

	/* get the physical adresses structures that should be mapped in userland */
	for(i = 0 ; i < SLOTS_NUMBER ; i++) {

		if (rm->adapter->rx_buffer_area[i].m_head == NULL){
#if (__RINGMAP_DEB) 
			printf(WARN_PREFIX"[%s] mbuf for descriptor=%d is not allocated\n", __func__, i);
			printf(WARN_PREFIX"[%s] The reason may be: ifnet structure for our network device not present or not initialized\n", __func__);
#endif
			return (-err);
		}

		rm->ring.slot[i].mbuf.kern = 	(vm_offset_t)			rm->adapter->rx_buffer_area[i].m_head;
		rm->ring.slot[i].mbuf.phys = 	(bus_addr_t)	vtophys(rm->adapter->rx_buffer_area[i].m_head);

		rm->ring.slot[i].packet.kern = 	(vm_offset_t)			rm->adapter->rx_buffer_area[i].m_head->m_data;
		rm->ring.slot[i].packet.phys =	(bus_addr_t)	vtophys(rm->adapter->rx_buffer_area[i].m_head->m_data);

		rm->ring.slot[i].descriptor.kern = 	(vm_offset_t)			&rm->adapter->rx_desc_base[i];
		rm->ring.slot[i].descriptor.phys = 	(bus_addr_t)	vtophys(&rm->adapter->rx_desc_base[i]);

#if (__RINGMAP_DEB)
		ringmap_print_slot(adapter, i);
#endif
	}

	rspp = (bus_addr_t)vtophys(&rm->ring);
	nic_statspp = (bus_addr_t)vtophys(&adapter->stats); 

	uiomove(&rspp, sizeof(bus_addr_t), uio);
	uiomove(&nic_statspp, sizeof(bus_addr_t), uio);

	RINGMAP_OUTPUT(end);
    return(0);
}


int
ringmap_ioctl (struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	int err = 0, err_sleep;
	struct adapter *adapter = (struct adapter *)get_adapter_struct(dev);
	struct ringmap *rm = adapter->rm;

	unsigned int *userp = NULL;

	RINGMAP_OUTPUT(begin);

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
			ringmap_enable_intr(adapter);
			ringmap_enable_receive(adapter);
		break;

		/* Disable Receive and Interrupts */
		case IOCTL_DISABLE_RECEIVE:
			RINGMAP_IOCTL(IOCTL_DISABLE_RECEIVE);
			ringmap_disable_intr(adapter);
			ringmap_disable_receive(adapter);
		break;
		
		/* Disable Flow Control */
		case IOCTL_DISABLE_FLOWCNTR:
			RINGMAP_IOCTL(IOCTL_DISABLE_FLOWCNTR);
			ringmap_disable_flowcontr(adapter);
		break;

		/* Sleep and wait for new frames */
		case IOCTL_SLEEP_WAIT:
			rm->ring.user_wait_kern++;
			SET_RDT(adapter);
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
        	
	RINGMAP_OUTPUT(end);
        	
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
		if (em_rxeof(adapter, adapter->rx_process_limit) != 0)
			taskqueue_enqueue(adapter->tq, &adapter->rxtx_task);
	}

#if (INTR_DEB) 	
	printf("########################################################################\n\n");
#endif

	ringmap_enable_intr(adapter);

	if (rm->procp != NULL) {
		wakeup(rm);

		if (KERN_TO_USER_RING_DISTANCE(&rm->ring) <= RING_SAFETY_MARGIN)
				callout_reset(&rm->ring_callout, SECS_TO_TICKS(SECS_WAIT_USER), ringmap_wdog, (void *)adapter);
	}
}

void
ringmap_wdog(void *arg)
{
	struct adapter *adapter;
	struct ringmap *rm;

	/* Dangerouse: it can very fast fill messages */
	RINGMAP_INTR(start);

	adapter = (struct adapter *)arg;
	rm = adapter->rm;

	/* Now we are waiting for user process. Update statistics */
	rm->ring.kern_wait_user++;

	if (KERN_TO_USER_RING_DISTANCE(&rm->ring) <= RING_SAFETY_MARGIN){
		wakeup(rm);
		if ((++rm->times_restart_callout) < RESTART_TIMER_COUNTER)
			callout_reset(&rm->ring_callout, SECS_TO_TICKS(SECS_WAIT_USER), ringmap_wdog, (void *)adapter);
	} else {
		rm->times_restart_callout = 0;
		SET_RDT(adapter); 		/* RDT = userrp - RING_SAFETY_MARGIN */
	}
}


struct adapter*
get_adapter_struct(struct cdev *dev)
{
	struct adapter *adapter;

	adapter = (struct adapter *)devclass_get_softc(em_devclass, dev2unit(dev));
	return (adapter);
}


/*
 * Return: 	1 - success
 * 			0 - otherwise
 */
int
ringmap_disable_receive(struct adapter *adapter)
{
	uint32_t	rctl;

	RINGMAP_FUNC_DEBUG(start);

	if (adapter == NULL){
		printf(ERR_PREFIX"[%s] NULL pointer to adapter structure\n", __func__);
		return (0);
	}
	rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	return (1);
}


/*
 * Return: 	1 - success
 * 			0 - otherwise
 */
int
ringmap_enable_receive(struct adapter *adapter)
{
	uint32_t	rctl;

	RINGMAP_FUNC_DEBUG(start);

	if (adapter == NULL){
		printf(ERR_PREFIX"[%s] NULL pointer to adapter structure\n", __func__);
		return (0);
	}
	rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl | E1000_RCTL_EN);

	return (1);
}


int
ringmap_enable_intr(struct adapter *adapter)
{
	struct e1000_hw *hw;
	uint32_t ims_mask = IMS_ENABLE_MASK;
	
	RINGMAP_FUNC_DEBUG(start);

	if (adapter == NULL){
		printf(ERR_PREFIX"[%s] NULL pointer to adapter structure\n", __func__);
		return (0);
	}
	hw = &adapter->hw;

	E1000_WRITE_REG(hw, E1000_IMS, ims_mask);
	
	return (1);
}


int
ringmap_disable_intr(struct adapter *adapter)
{
	struct e1000_hw *hw;
	
	RINGMAP_FUNC_DEBUG(start);

	if (adapter == NULL){
		printf(ERR_PREFIX"[%s] NULL pointer to adapter structure\n", __func__);
		return (0);
	}
	hw = &adapter->hw;

	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
	
	return (1);
}


void
ringmap_print_ring (struct adapter *adapter, int level)
{
	struct ringmap *rm = adapter->rm;

	printf("Ring Size = %d \n",rm->ring.size );	
	printf("Times Kern wait for User = %llu \n",rm->ring.kern_wait_user);	
	printf("Times User wait for Kern = %llu \n",rm->ring.user_wait_kern);	
	printf("Interrupts Counter = %llu \n",rm->ring.interrupts_counter);	

	ringmap_print_ring_pointers(adapter);
}


void 
ringmap_print_slot(struct adapter *adapter, unsigned int slot_number)
{
	struct ringmap *rm = adapter->rm;

	printf("Slot Number: %d \n", slot_number);
	printf("---------------- \n");

	printf("[%s] physical addr of descriptor[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring.slot[slot_number].descriptor.phys);
	printf("[%s] kernel addr of descriptor[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring.slot[slot_number].descriptor.kern);
	printf("[%s] physical addr of mbuf[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring.slot[slot_number].mbuf.phys);
	printf("[%s] kernel addr of mbuf[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring.slot[slot_number].mbuf.kern);
	printf("[%s] physical addr of packet_buffer[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring.slot[slot_number].packet.phys);
	printf("[%s] kernel addr of packet_buffer[%d] = 0x%X\n", __func__, slot_number, 
			(unsigned int) rm->ring.slot[slot_number].packet.kern);
	printf(" \n");
}


int 
ringmap_print_ring_pointers(struct adapter *adapter)
{
	unsigned int rdt, rdh;
	struct ringmap *rm = adapter->rm;

	rdh = E1000_READ_REG(&adapter->hw, E1000_RDH(0));
	rdt = E1000_READ_REG(&adapter->hw, E1000_RDT(0));

	printf("\n  +++++++++  RING POINTERS  ++++++++++++ \n");
	printf("  +  RDH = %d (KERN POINTER)\n", rdh);
	printf("  +  RDT = %d (USER POINTER)\n", rdt);
	printf("  +\n");
	printf("  +  kernrp = %d \n", rm->ring.kernrp);
	printf("  +  userrp = %d \n", rm->ring.userrp);
	printf("  ++++++++++++++++++++++++++++++++++++++ \n\n");

	return (0);
}


void 
ringmap_disable_flowcontr(struct adapter *adapter)
{
	unsigned int ctrl; 

	ctrl = E1000_READ_REG(&(adapter)->hw, E1000_CTRL);
	ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
	E1000_WRITE_REG(&(adapter)->hw, E1000_CTRL, ctrl);
}
