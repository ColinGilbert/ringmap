#define DESC_AREA(adapter)	(adapter)->rx_desc_base
#define MBUF_AREA(adapter)	(adapter)->rx_buffer_area

/* Kernel address of mbuf wich placed in the slot "i" */
#define GET_MBUF_P(adapter, i)		\
	(MBUF_AREA(adapter)[(i)].m_head)


/* Kernel address of the packet wich placed in the slot "i" */
#define GET_PACKET_P(adapter, i)		\
	(MBUF_AREA(adapter)[(i)].m_head->m_data)


/* Kernel address of the descriptor wich placed in the slot "i" */
#define GET_DESCRIPTOR_P(adapter, i)	\
	(&(DESC_AREA(adapter)[(i)]))


/* Registers access */
#define RINGMAP_HW_READ_REG E1000_READ_REG
#define RINGMAP_HW_WRITE_REG E1000_WRITE_REG



#define RINGMAP_HW_READ_HEAD(adapter)						\
		RINGMAP_HW_READ_REG(&(adapter)->hw, E1000_RDH(0))	

#define RINGMAP_HW_SYNC_HEAD(adapter, ring)					\
		SW_HEAD(ring) = RINGMAP_HW_READ_HEAD(adapter);	

#define RINGMAP_HW_SYNC_TAIL(adapter, ring)				\
		RINGMAP_HW_WRITE_REG(&(adapter)->hw, E1000_RDT(0), (ring)->userrp)

#define RINGMAP_HW_WRITE_TAIL(adapter, val)				\
		RINGMAP_HW_WRITE_REG(&(adapter)->hw, E1000_RDT(0), (val))

#define RINGMAP_HW_READ_TAIL(adapter)						\
		RINGMAP_HW_READ_REG(&(adapter)->hw, E1000_RDT(0))

