#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/bpf.h>

#include <net/ringmap.h>

#include "e1000_api.h"
#include "if_lem.h"
#include "ringmap_8254.h"

int rm_8254_set_slot(struct capt_object *, unsigned int);
void rm_8254_delayed_interrupt(void *, struct ringmap *);
int rm_8254_print_ring_pointers(struct adapter *);
void rm_8254_sync_tail(struct capt_object *);
void rm_8254_sync_head(struct capt_object *);
struct capt_object * rm_8254_find_next(struct adapter *);
int rm_8254_set_queue(struct capt_object *, unsigned int);

extern devclass_t em_devclass;
extern void ringmap_print_slot(struct ring *, unsigned int);
extern void print_capt_obj(struct capt_object *);


struct ringmap_functions ringmap_8254_functions = {
	.isr = NULL,
	.delayed_isr = rm_8254_delayed_interrupt,
	.per_packet_iteration = NULL,
	.sync_tail = rm_8254_sync_tail,
	.sync_head = rm_8254_sync_head,
	.set_slot = rm_8254_set_slot,
	.set_queue = rm_8254_set_queue,
	.pkt_filter = NULL,
};


/* 
 * Write the userrp into the RDT register
 * 2. SYNC_TAIL: RDT =  ring->userrp
 */
void 
rm_8254_sync_tail(struct capt_object *co)
{
	struct adapter *adapter = (struct adapter *)device_get_softc(co->rm->dev);

	RINGMAP_HW_SYNC_TAIL(adapter, co->ring);
}


/* Set value from RDH to the ring->kernrp*/
void 
rm_8254_sync_head(struct capt_object *co)
{
	struct adapter *adapter = (struct adapter *)device_get_softc(co->rm->dev);

	RINGMAP_HW_SYNC_HEAD(adapter, co->ring); 
}


void 
rm_8254_delayed_interrupt(void *context, struct ringmap *rm)
{
	struct adapter	*adapter = (struct adapter *)context;
	struct capt_object *co = NULL;
	struct timeval	last_ts;

	RINGMAP_INTR(start);

	RINGMAP_LOCK(adapter->rm);
	/* Do the next steps only if there is capturing process */ 
	if (adapter->rm->open_cnt > 0) {
		getmicrotime(&last_ts);
		rm_8254_sync_tail(rm_8254_find_next(adapter));

		SLIST_FOREACH(co, &adapter->rm->object_list, objects) {
			if (co->ring != NULL) {
#if (RINGMAP_INTR_DEB)
				PRINT_RING_PTRS(co->ring);
#endif
				co->ring->last_ts = last_ts;
				++co->ring->intr_num;
			}
		}
	}
	RINGMAP_UNLOCK(adapter->rm);

	RINGMAP_INTR(end);
}


/* 
 * Returns the capturing object which ring's TAIL pointer is mostly near to to
 * the HEAD(RDH) This is for the case we have more then one thread capturing
 * from the same interface. I am not sure we need it!
 */
struct capt_object *
rm_8254_find_next(struct adapter *adapter)
{
	unsigned int rdh, rdt, dist, min_dist = SLOTS_NUMBER;
	struct ringmap *rm = adapter->rm;
	struct capt_object *co = NULL, *min_co = NULL;

	rdh = RINGMAP_HW_READ_HEAD(adapter);

	SLIST_FOREACH(co, &rm->object_list, objects) {
		rdt = co->ring->userrp;
		dist = R_DISTANCE(rdh, rdt);
		if (dist <= min_dist) {
			min_dist = dist;
			min_co = co;
		}
	}

	return (min_co);
}


int
rm_8254_set_slot(struct capt_object *co, unsigned int slot_num)
{
	device_t dev = NULL; 	
	struct adapter *adapter = NULL; 
	struct ring *ring = NULL; 

#if (__RINGMAP_DEB)
	printf("[%s] Set slot: %d\n", __func__, slot_num);
#endif

	dev = co->rm->dev;
	adapter = (struct adapter *)device_get_softc(dev);
	ring = co->ring;

	if (GET_MBUF_P(adapter, slot_num) == NULL){
		RINGMAP_ERROR(Pointer to mbuf is NULL);
		goto fail;
	}
	if (GET_PACKET_P(adapter, slot_num) == NULL){
		RINGMAP_ERROR(Pointer to packet is NULL);
		goto fail;
	}
	if (GET_DESCRIPTOR_P(adapter, slot_num) == NULL){
		RINGMAP_ERROR(Pointer to descriptor is NULL);
		goto fail;
	}

	/* Now if everything is Ok, we can initialize ring pointers */
	ring->slot[slot_num].mbuf.kern = 
		(vm_offset_t)GET_MBUF_P(adapter, slot_num);
	ring->slot[slot_num].mbuf.phys = 
		(bus_addr_t)vtophys(GET_MBUF_P(adapter, slot_num));

	ring->slot[slot_num].packet.kern = 
		(vm_offset_t)GET_PACKET_P(adapter, slot_num);
	ring->slot[slot_num].packet.phys = 
		(bus_addr_t)vtophys(GET_PACKET_P(adapter, slot_num));

	ring->slot[slot_num].descriptor.kern = 
		(vm_offset_t)GET_DESCRIPTOR_P(adapter, slot_num);
	ring->slot[slot_num].descriptor.phys = 
		(bus_addr_t)vtophys(GET_DESCRIPTOR_P(adapter, slot_num));

	return (0);

fail:
	RINGMAP_ERROR(Probably you have to do: ifconfig up);
	return (-1);
} 


/* Print the values from RDT and RDH */
int 
rm_8254_print_ring_pointers(struct adapter *adapter)
{
	unsigned int rdt, rdh;
	struct ringmap *rm = NULL;

	rm = adapter->rm;
	
	if (rm == NULL)
		goto out;

	rdh = RINGMAP_HW_READ_HEAD(adapter);
	rdt = RINGMAP_HW_READ_TAIL(adapter);

	printf("\n==  +++++++++  RING POINTERS  ++++++++++++ \n");
	printf("==  +  HW HEAD = %d (KERN POINTER)\n", rdh);
	printf("==  +  HW TAIL = %d (USER POINTER)\n", rdt);
	printf("==  ++++++++++++++++++++++++++++++++++++++ \n\n");

out:
	return (0);
}


/* 8254x controllers have not multiqueue support: que = NULL */
int 
rm_8254_set_queue(struct capt_object *co, unsigned int i)
{
	/* No multiqueue for 8254 */
	co->que = NULL;

	/* Interrupts source is interface */
	co->intr_context = device_get_softc(co->rm->dev);

	return (0);
}
