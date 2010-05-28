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


/*  
 * Set hardware USER pointer register (RDT) behind the 
 * user pointer on RING_SAFETY_MARGIN entities
 */
#define SET_RDT(adapter)			\
		do {						\
		E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), R_MODULO((adapter->rm->ring.userrp) - RING_SAFETY_MARGIN, SLOTS_NUMBER));	\
		adapter->rm->ring.hw_RDT = R_MODULO((adapter->rm->ring.userrp) - RING_SAFETY_MARGIN, SLOTS_NUMBER);	     				\
		} while(0);
