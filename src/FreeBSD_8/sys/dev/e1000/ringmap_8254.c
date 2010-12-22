#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/conf.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <net/ringmap.h>

#include "e1000_api.h"
#include "if_lem.h"

int rm_8254_set_queue(struct capt_object *, unsigned int);
unsigned int rm_8245_get_rdh(void *hw_ring);
unsigned int rm_8245_get_rdt(void *hw_ring);
void rm_8245_set_rdt(unsigned int val, void *hw_ring);
struct mbuf * rm_8254_get_mbuf(void *buffer_area, unsigned int num);
vm_offset_t rm_8254_get_packet(void *buffer_area, unsigned int num);
vm_offset_t rm8254_get_rx_desc(void * rx_desc_area, unsigned int num);
int rm_8245_get_queuesnum(void);
void rm_8254_receive_disable (struct ringmap *rm);
void rm_8254_receive_enable (struct ringmap *rm);
void rm_8254_intr_disable (struct ringmap *rm);
void rm_8254_intr_enable (struct ringmap *rm);
unsigned int rm_8254_get_slotsnum(struct ring *);
void rm_8254_set_slotsnum(struct ring *);


extern devclass_t em_devclass;


struct ringmap_functions ringmap_8254_functions = {
	.isr = NULL,
	.delayed_isr = NULL,
	.per_packet_iteration = NULL,
	.get_head = rm_8245_get_rdh,
	.get_tail = rm_8245_get_rdt,
	.set_tail = rm_8245_set_rdt,
	.get_mbuf = rm_8254_get_mbuf,
	.get_packet = rm_8254_get_packet,
	.set_queue = rm_8254_set_queue,
	.pkt_filter = NULL,
	.get_queuesnum = rm_8245_get_queuesnum,
	.set_timestamp = NULL,
	.receive_disable = rm_8254_receive_disable,
	.receive_enable = rm_8254_receive_enable,
	.intr_disable = rm_8254_intr_disable,
	.intr_enable = rm_8254_intr_enable,
	.get_slotsnum = rm_8254_get_slotsnum,
	.set_slotsnum = rm_8254_set_slotsnum,
};


void
rm_8254_intr_disable (struct ringmap *rm)
{
	struct adapter *adapter =(struct adapter *)device_get_softc(rm->dev);
	struct e1000_hw *hw = &adapter->hw;

	if (adapter->msix)
		E1000_WRITE_REG(hw, EM_EIAC, 0);
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);
}


void
rm_8254_intr_enable (struct ringmap *rm)
{
	struct adapter *adapter =(struct adapter *)device_get_softc(rm->dev);
	struct e1000_hw *hw = &adapter->hw;
	u32 ims_mask = IMS_ENABLE_MASK;

	if (adapter->msix) {
		E1000_WRITE_REG(hw, EM_EIAC, EM_MSIX_MASK);
		ims_mask |= EM_MSIX_MASK;
	} 
	E1000_WRITE_REG(hw, E1000_IMS, ims_mask);
}


void
rm_8254_receive_disable (struct ringmap *rm)
{
	struct adapter *adapter; 
	u32	rctl;

	adapter = (struct adapter *)device_get_softc(rm->dev);
	rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);
}


void
rm_8254_receive_enable (struct ringmap *rm)
{
	struct adapter *adapter; 
	u32	rctl;

	adapter = (struct adapter *)device_get_softc(rm->dev);
	rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl | E1000_RCTL_EN);
}


struct mbuf * 
rm_8254_get_mbuf(void *buffer_area, unsigned int num)
{
	struct em_buffer *buf = (struct em_buffer *)buffer_area;
	return (buf[num].m_head);
}


int 
rm_8245_get_queuesnum()
{
	return (1);
}


vm_offset_t
rm_8254_get_packet(void *buffer_area, unsigned int num)
{
	struct mbuf *mb = rm_8254_get_mbuf(buffer_area, num);
	return ((vm_offset_t)mb->m_data);
}


unsigned int
rm_8245_get_rdh(void *hw_ring)
{
	return (E1000_READ_REG((struct e1000_hw *)hw_ring, E1000_RDH(0)));
}


unsigned int
rm_8245_get_rdt(void *hw_ring)
{
	return (E1000_READ_REG((struct e1000_hw *)hw_ring, E1000_RDT(0)));
}


void
rm_8245_set_rdt(unsigned int val, void *hw_ring)
{
	E1000_WRITE_REG((struct e1000_hw *)hw_ring, E1000_RDT(0), val);
}


int 
rm_8254_set_queue(struct capt_object *co, unsigned int i)
{

	device_t dev = NULL; 	
	struct adapter *adapter = NULL; 

	dev = co->rm->dev;
	adapter = (struct adapter *)device_get_softc(dev);


	co->hw_rx_ring = &adapter->hw;
	co->hw_tx_ring = &adapter->hw;

	co->rx_buffers = adapter->rx_buffer_area;
	co->tx_buffers = adapter->tx_buffer_area;

	co->rx_desc_base = adapter->rx_desc_base;
	co->tx_desc_base = adapter->tx_desc_base;

	/* Interrupt context is adapter structure. Look in if_lem.h */
	co->intr_context = device_get_softc(co->rm->dev);

	return (0);
}


unsigned int rm_8254_get_slotsnum(struct ring *ring)
{
	return (ring->size);	
}


void rm_8254_set_slotsnum(struct ring *ring)
{
	ring->size = SLOTS_NUMBER;
}
