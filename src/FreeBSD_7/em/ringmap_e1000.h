#ifdef _KERNEL
#define RINGMAP_HW_WRITE_REG E1000_WRITE_REG


#define RINGMAP_HW_READ_REG E1000_READ_REG


/* RDT = (userrp - RING_SAFETY_MARGIN) mod SLOTS_NUMBER */
#define RINGMAP_HW_SYNC_TAIL SET_RDT


/* Enable packets receive */
#define RINGMAP_HW_ENABLE_RECEIVE(adapter)							\
	do {															\
		RINGMAP_HW_WRITE_REG(	&adapter->hw, 						\
								E1000_RCTL, 						\
								RINGMAP_HW_READ_REG(&adapter->hw, 	\
								E1000_RCTL) | E1000_RCTL_EN);		\
		} while(0);


/* Enable inrrupts on adapter */
#define RINGMAP_HW_ENABLE_INTR(adapter)					\
	do {												\
			RINGMAP_HW_WRITE_REG(	&adapter->hw, 		\
									E1000_IMS, 			\
									IMS_ENABLE_MASK);	\
		} while(0);


#define RINGMAP_HW_DISABLE_INTR(adapter)							\
	do {															\
		RINGMAP_HW_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);	\
	} while(0);


#define RINGMAP_HW_DISABLE_RECEIVE(adapter)							\
	do {															\
		RINGMAP_HW_WRITE_REG( 	&adapter->hw, 						\
								E1000_RCTL, 						\
								(RINGMAP_HW_READ_REG(&adapter->hw, E1000_RCTL)) & (~E1000_RCTL_EN));	\
	} while(0);


#define RINGMAP_HW_READ_HEAD(adapter)						\
		RINGMAP_HW_READ_REG(&adapter->hw, E1000_RDH(0))	


#define RINGMAP_HW_READ_TAIL(adapter)						\
		RINGMAP_HW_READ_REG(&adapter->hw, E1000_RDT(0))


#define RINGMAP_GET_ADAPTER_STRUCT(adapter)	\
	(struct adapter *)devclass_get_softc(em_devclass, dev2unit(dev))


#define RINGMAP_HW_DISABLE_FLOWCONTR(adapter)		\
	do {											\
		RINGMAP_HW_WRITE_REG(	&(adapter)->hw, 	\
								E1000_CTRL, 		\
								(RINGMAP_HW_READ_REG(&(adapter)->hw, E1000_CTRL)) & (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE))	\
							);	\
	} while(0);


/* Kernel address of mbuf wich placed in the slot "i" */
#define RINGMAP_GET_MBUF(adapter, i)		\
	((adapter)->rx_buffer_area[(i)].m_head)


/* Kernel address of the packet wich placed in the slot "i" */
#define RINGMAP_GET_PACKET(adapter, i)		\
	((adapter)->rx_buffer_area[(i)].m_head->m_data)


/* Kernel address of the descriptor wich placed in the slot "i" */
#define RINGMAP_GET_DESCRIPTOR(adapter, i)	\
	(&((adapter)->rx_desc_base[(i)]))


/*  
 * Set hardware USER pointer register (RDT) behind the 
 * user pointer on RING_SAFETY_MARGIN entities
 */
#define SET_RDT(adapter)			\
		do {						\
		E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), R_MODULO((adapter->rm->ring->userrp) - RING_SAFETY_MARGIN, SLOTS_NUMBER));	\
		adapter->rm->ring->hw_RDT = R_MODULO((adapter->rm->ring->userrp) - RING_SAFETY_MARGIN, SLOTS_NUMBER);	     				\
		} while(0);

#endif /*_KERNEL*/



#ifndef _KERNEL

typedef uint64_t u64;

struct e1000_hw_stats {
	u64 crcerrs;
	u64 algnerrc;
	u64 symerrs;
	u64 rxerrc;
	u64 mpc;
	u64 scc;
	u64 ecol;
	u64 mcc;
	u64 latecol;
	u64 colc;
	u64 dc;
	u64 tncrs;
	u64 sec;
	u64 cexterr;
	u64 rlec;
	u64 xonrxc;
	u64 xontxc;
	u64 xoffrxc;
	u64 xofftxc;
	u64 fcruc;
	u64 prc64;
	u64 prc127;
	u64 prc255;
	u64 prc511;
	u64 prc1023;
	u64 prc1522;
	u64 gprc;
	u64 bprc;
	u64 mprc;
	u64 gptc;
	u64 gorc;
	u64 gotc;
	u64 rnbc;
	u64 ruc;
	u64 rfc;
	u64 roc;
	u64 rjc;
	u64 mgprc;
	u64 mgpdc;
	u64 mgptc;
	u64 tor;
	u64 tot;
	u64 tpr;
	u64 tpt;
	u64 ptc64;
	u64 ptc127;
	u64 ptc255;
	u64 ptc511;
	u64 ptc1023;
	u64 ptc1522;
	u64 mptc;
	u64 bptc;
	u64 tsctc;
	u64 tsctfc;
	u64 iac;
	u64 icrxptc;
	u64 icrxatc;
	u64 ictxptc;
	u64 ictxatc;
	u64 ictxqec;
	u64 ictxqmtc;
	u64 icrxdmtc;
	u64 icrxoc;
	u64 cbtmpc;
	u64 htdpmc;
	u64 cbrdpc;
	u64 cbrmpc;
	u64 rpthc;
	u64 hgptc;
	u64 htcbdpc;
	u64 hgorc;
	u64 hgotc;
	u64 lenerrs;
	u64 scvpc;
	u64 hrmpc;
};
#endif
