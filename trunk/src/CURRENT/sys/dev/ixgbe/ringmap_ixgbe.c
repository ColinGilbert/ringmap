#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/atomic.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <net/ringmap.h>

#include "ixgbe_api.h"
#include "ixgbe.h"
#include "ringmap_8259.h"


/* External things */
extern devclass_t ixgbe_devclass;

extern void	ixgbe_enable_intr(struct adapter *);
extern void	ixgbe_disable_intr(struct adapter *);
extern void	ixgbe_refresh_mbufs(struct rx_ring *, int);

extern void ringmap_print_slot(struct ring *, unsigned int);
extern void print_capt_obj(struct capt_object *);
/*******************/


int rm_8259_set_ringmap_to_adapter(device_t, struct ringmap *);
struct ringmap * rm_8259_get_ringmap_p(device_t);
device_t rm_8259_get_device_p(struct cdev *);
void rm_8259_enable_intr(device_t);
void rm_8259_disable_intr(device_t);
int rm_8259_set_slot(struct capt_object*, unsigned int);
int rm_8259_set_queue(struct capt_object *, unsigned int);
void rm_8259_interrupt(void *);
void rm_8259_delayed_interrupt(void *);
int rm_8259_print_ring_pointers(void *);
void rm_8259_sync_tail(void *);
void rm_8259_sync_head(void *);
void rm_8259_delayed_interrupt_per_packet(void *, int);
void rm_8259_disable_receive(void *);
void rm_8259_enable_receive(void *);
struct ix_queue * rm_8259_get_free_queue(device_t);
struct capt_object * get_capt_obj(void *context);


struct ringmap_functions ringmap_8259_f = {
	rm_8259_set_ringmap_to_adapter,
	rm_8259_enable_intr,
	rm_8259_disable_intr,
	rm_8259_interrupt,
	rm_8259_delayed_interrupt,
	rm_8259_delayed_interrupt_per_packet,
	rm_8259_sync_tail,
	rm_8259_sync_head,
	rm_8259_set_slot,
	rm_8259_set_queue,
	rm_8259_get_ringmap_p,
	rm_8259_get_device_p
};



/*
 * Set pointer to ringmap in the adapter structure.
 */
int
rm_8259_set_ringmap_to_adapter(device_t dev, struct ringmap *rm)
{
	struct adapter *adapter;

	adapter = (struct adapter *)device_get_softc(dev);
	adapter->rm = rm;

#if (__RINGMAP_DEB)
	printf(RINGMAP_PREFIX"Number of queeus on adapter:  %d\n", 
			adapter->num_queues);
#endif 
	return (0);
}


void 
rm_8259_sync_tail(void *context)
{
	struct ix_queue	*que	= (struct ix_queue *)context;
	struct adapter *adapter	= que->adapter;
	struct capt_object *co 	= NULL;
	
	RINGMAP_LOCK(adapter->rm);
	co = get_capt_obj(que);
	if (co != NULL) {
		RINGMAP_HW_SYNC_TAIL(que, co->ring);	
	}
	RINGMAP_UNLOCK(adapter->rm);
}


void 
rm_8259_sync_head(void *context)
{
	struct ix_queue	*que	= (struct ix_queue *)context;
	struct adapter *adapter	= que->adapter;
	struct capt_object *co 	= NULL;


	RINGMAP_LOCK(adapter->rm);
	co = get_capt_obj(que);
	if (co != NULL) {
		RINGMAP_HW_SYNC_HEAD(que, co->ring);
	}
	RINGMAP_UNLOCK(adapter->rm);
}


/* 
 * This should be called from ISR. Other interrupts are disallowed!
 * It means the functions must be as small as possible
 */
void 
rm_8259_interrupt(void *arg)
{
	struct adapter	*adapter = (struct adapter *) arg;

	/* count interrupts only if there is capturing object */
	if ( adapter->rm->open_cnt > 0 )
		adapter->rm->interrupts_counter++;
}


void 
rm_8259_delayed_interrupt(void *context)
{
	struct ix_queue	*que	= (struct ix_queue *)context;
	struct adapter	*adapter = (struct adapter *)que->adapter;
	struct capt_object *co = NULL;


	RINGMAP_LOCK(adapter->rm);

	adapter->rm->interrupts_counter++;

	if ( adapter->rm->open_cnt > 0 ) {
		co = get_capt_obj(que);
		if (co != NULL) {
			co->ring->intr_num = que->irqs;
			getmicrotime(&co->ring->last_ts);
			rm_8259_sync_tail(context);
		}
	}
	RINGMAP_UNLOCK(adapter->rm);
}


void 
rm_8259_delayed_interrupt_per_packet(void *context, int slot_num)
{
	struct ix_queue	*que	= (struct ix_queue *)context;
	struct adapter	*adapter = (struct adapter *)que->adapter;
	struct ixgbe_rx_buf	*rxbuf;
	struct ringmap *rm = adapter->rm;;
	struct capt_object *co = NULL;

	RINGMAP_INTR(start);

	RINGMAP_LOCK(rm);

	if (slot_num >= SLOTS_NUMBER){
		RINGMAP_ERROR(STOP! ERROR! Unallowed slot Number!);
		goto out;
	}

	rm_8259_print_ring_pointers(que);

	if (adapter->rm->open_cnt) {
		co = get_capt_obj(que);
		if (co != NULL) {

			co->ring->slot[slot_num].intr_num = co->ring->intr_num;
			co->ring->slot[slot_num].ts = co->ring->last_ts;
			co->ring->slot[slot_num].is_ok = 1;

			rxbuf = &que->rxr->rx_buffers[slot_num];

#if (RINGMAP_INTR_DEB)
			rxbuf->m_pack = (struct mbuf *)K_MBUF(co->ring, slot_num);
			rxbuf->m_pack->m_data = (void *)K_PACKET(co->ring, slot_num);
			que->rxr->rx_base[slot_num].read.pkt_addr = 
				htole64(vtophys(K_PACKET(co->ring, slot_num)));
			printf("---------------------------------------------------- \n");
			printf(RINGMAP_PREFIX"[%s] Slot = %d\n", __func__, slot_num);
			PRINT_SLOT(co->ring, slot_num);	
			PRINT_RING_PTRS(co->ring);

			printf("[%s] rxbuf->m_pack [%d] : 0x%X\n", 
					__func__, slot_num, (unsigned int)rxbuf->m_pack);
			printf("[%s] pckt phys addr [%d] : 0x%llX\n", 
					__func__, slot_num, 
					que->rxr->rx_base[slot_num].read.pkt_addr);
			printf("---------------------------------------------------- \n");
#endif
		}
	}

out:

	RINGMAP_UNLOCK(rm);

	RINGMAP_INTR(end);
}


int
rm_8259_set_slot(struct capt_object *co, unsigned int slot_num)
{
	struct ix_queue	*que	= co->que;
	struct ring *ring = co->ring;

#if (__RINGMAP_DEB)
	printf("[%s] Set slot: %d\n", __func__, slot_num);
#endif

	/* First check the pointers */
	if (que == NULL) {
		RINGMAP_ERROR(Null pointer to the queue);
		goto fail;
	}
	if (GET_MBUF_P(que, slot_num) == NULL){
		RINGMAP_ERROR(Pointer to mbuf is NULL);
		goto fail;
	}
	if (GET_PACKET_P(que, slot_num) == NULL){
		RINGMAP_ERROR(Pointer to packet is NULL);
		goto fail;
	}

	/* Now if everything is Ok, we can initialize slots variables */
	ring->slot[slot_num].mbuf.kern = 
		(vm_offset_t)GET_MBUF_P(que, slot_num);
	ring->slot[slot_num].mbuf.phys = 
		(vm_paddr_t)vtophys(GET_MBUF_P(que, slot_num));

	ring->slot[slot_num].packet.kern = 
		(vm_offset_t)GET_PACKET_P(que, slot_num);
	ring->slot[slot_num].packet.phys = 
		(vm_paddr_t)vtophys(GET_PACKET_P(que, slot_num));

	return (0);

fail:

	return (-1);
}


/*
 * Disable interrupts on adapter 
 */
void 
rm_8259_disable_intr(device_t dev)
{
	
	struct adapter *adapter;
	adapter = (struct adapter *)device_get_softc(dev);

	/*Use function implemeted in native (em) driver */
	ixgbe_disable_intr(adapter);
}


/*
 * Enable interrupts on adapter 
 */
void 
rm_8259_enable_intr(device_t dev)
{
	
	struct adapter *adapter;
	adapter = (struct adapter *)device_get_softc(dev);

	/*Use function implemeted in native (em) driver */
	ixgbe_enable_intr(adapter);
}


/*
 * Get pointer to device structure of adapter using our ringmap char device. 
 * This is a trick. Our cdev must have the same unit number as dev of adapter.
 * Look in ringmap.c: ringmap_attach() where we create our cdev. 
 */
device_t 
rm_8259_get_device_p(struct cdev *cdev)
{
	struct adapter *adapter;

	adapter = (struct adapter *)devclass_get_softc(ixgbe_devclass, dev2unit(cdev));
#if (__RINGMAP_DEB)
	if (adapter == NULL){
		RINGMAP_WARN(Can not get pointer to adapter structure);
	}
#endif 

	return (adapter->dev);
}


/*
 * Returns pointer to ringmap structure
 */
struct ringmap * 
rm_8259_get_ringmap_p(device_t dev)
{
	struct adapter *adapter;

	adapter = (struct adapter *)device_get_softc(dev);
	return (adapter->rm);
}


void 
rm_8259_disable_receive(void *context)
{
	u32 rxctrl;
 
	struct ix_queue	*que	= (struct ix_queue *)context;
	struct adapter	*adapter = (struct adapter *)que->adapter;
	struct ixgbe_hw	*hw = &adapter->hw;
	
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL,
	    rxctrl & ~IXGBE_RXCTRL_RXEN);
}


void 
rm_8259_enable_receive(void *context)
{
	u32 rxctrl;
 
	struct ix_queue	*que	= (struct ix_queue *)context;
	struct adapter	*adapter = (struct adapter *)que->adapter;
	struct ixgbe_hw	*hw = &adapter->hw;
	
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL,
	    rxctrl | IXGBE_RXCTRL_RXEN);
}


/* Return a queue that is not associate with any capturing objects */
struct ix_queue * 
rm_8259_get_free_queue(device_t dev)
{
	struct adapter 	*adapter;
	struct ringmap *rm =NULL;
	struct ix_queue *que = NULL;
	struct capt_object *co = NULL;
	int i, j = 1;

	adapter = (struct adapter *)device_get_softc(dev);
	que = adapter->queues;
	rm = adapter->rm;

	for (i = 0; (i < adapter->num_queues); i++, que++) {
		j = 0;
		SLIST_FOREACH(co, &rm->object_list, objects) {
			j += (co->que == que);
		}
		if (j == 0)
			return (que);
	}
	
	return (NULL);
}


/* Associate the capturing object with the queue */
int 
rm_8259_set_queue(struct capt_object *co, unsigned int queue_num)
{
	device_t dev;
	struct adapter *adapter;
	int err = -1;

	RINGMAP_FUNC_DEBUG(start);
		
	if (co->rm != NULL) {
		dev = co->rm->dev;
		adapter = (struct adapter *)device_get_softc(dev);

#if (__RINGMAP_DEB)
		printf("[%s] Before initialization\n", __func__);
		print_capt_obj(co);
#endif 
		if (queue_num < adapter->num_queues) {
			co->que = &(adapter->queues[queue_num]);
			err = 0;
		} else {
			RINGMAP_ERROR(Wrong queue number);
		}
	} else {
		RINGMAP_ERROR(Capturing object is not associated with ringmap);
	}

#if (__RINGMAP_DEB)
		printf("[%s] After initialization\n", __func__);
		print_capt_obj(co);
#endif 
		
	RINGMAP_FUNC_DEBUG(end);

	return (err);
}


struct capt_object *
get_capt_obj(void *queue)
{
	struct ix_queue	*que	= (struct ix_queue *)queue;
	struct adapter *adapter	= que->adapter;
	struct ringmap *rm		= adapter->rm;
	struct capt_object *co = NULL; 

	SLIST_FOREACH(co, &rm->object_list, objects) {
		if (co->que == queue)
			return (co);
	}

	return (co);
}


int 
rm_8259_print_ring_pointers(void *context)
{
	struct ix_queue	*que	= (struct ix_queue *)context;
	unsigned int rdt, rdh;

	rdt = HW_READ_TAIL(que);
	rdh = HW_READ_HEAD(que); 

	printf("\n==  +++++++++  RING POINTERS  ++++++++++++ \n");
	printf("==  +  Queue Number: %d\n", que->rxr->me);
	printf("==  + \n");
	printf("==  +  HW TAIL = %d (USER POINTER)\n", rdt);
	printf("==  +  HW HEAD = %d (KERN POINTER)\n", rdh);
	printf("==  ++++++++++++++++++++++++++++++++++++++ \n\n");

	return (0);
}
