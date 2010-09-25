#define DESC_AREA(que)	(que)->rxr->rx_base
#define MBUF_AREA(que)	(que)->rxr->rx_buffers


/* Kernel address of mbuf wich placed in the slot "i" */
#define GET_MBUF_P(que, num)			\
	(MBUF_AREA(que)[(num)].m_pack)


/* Kernel address of the packet wich placed in the slot "i" */
#define GET_PACKET_P(que, num)		\
	(MBUF_AREA(que)[(num)].m_pack->m_data)


/* Registers access */
#define RINGMAP_HW_READ_REG IXGBE_READ_REG
#define RINGMAP_HW_WRITE_REG IXGBE_WRITE_REG

#define HW_RDT(que) IXGBE_RDT((que)->rxr->me)
#define HW_RDH(que) IXGBE_RDH((que)->rxr->me)

#define HW_STRUCT(que)	(&(que)->adapter->hw)

#define HW_READ_REG(que, reg)							\
	RINGMAP_HW_READ_REG(HW_STRUCT(que), (reg))

#define HW_WRITE_REG(que, reg, val)						\
	RINGMAP_HW_WRITE_REG(HW_STRUCT(que), (reg), (val))


#define HW_READ_HEAD(que)						\
		HW_READ_REG((que), HW_RDH(que))

#define HW_READ_TAIL(que)						\
		HW_READ_REG((que), HW_RDT(que))

#define HW_WRITE_TAIL(que, val)					\
		HW_WRITE_REG((que), HW_RDT(que), (val))


#define RINGMAP_HW_SYNC_HEAD(que, ring)					\
		SW_HEAD(ring) = HW_READ_HEAD(que);	

#define RINGMAP_HW_SYNC_TAIL(que, ring)					\
		HW_WRITE_TAIL((que), SW_TAIL(ring))

