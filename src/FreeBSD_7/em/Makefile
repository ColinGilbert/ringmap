#$FreeBSD$
.PATH:  ${.CURDIR}

# The name of ringmap network device
KMOD    = if_ringmap

SRCS    = device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += if_em.c $(SHARED_SRCS)
SHARED_SRCS = e1000_api.c e1000_phy.c e1000_nvm.c e1000_mac.c e1000_manage.c
SHARED_SRCS += e1000_80003es2lan.c e1000_82542.c e1000_82541.c e1000_82543.c
SHARED_SRCS += e1000_82540.c e1000_ich8lan.c e1000_82571.c e1000_osdep.c
SHARED_SRCS += e1000_82575.c 

# ringmap source code 
SHARED_SRCS+=ringmap.c

# This macro will used for adding our ringmap-code in if_em files
CFLAGS+=-D__RINGMAP__

# Debug output. Uncommet it for debugging goals
#CFLAGS+=-D__FIVEG_DA__DEB
#CFLAGS+=-DINTR_DEB
#CFLAGS+=-D__INIT_DEB



# Uncomment this to disable Fast interrupt handling.
# and enable legacy interrupt handling
#CFLAGS  += -DEM_LEGACY_IRQ

# This option enables IEEE 1588 Precision Time Protocol
#CFLAGS += -DEM_TIMESYNC

# DEVICE_POLLING for a non-interrupt-driven method
#CFLAGS  += -DDEVICE_POLLING

clean:
	rm -f opt_bdg.h device_if.h bus_if.h pci_if.h setdef*
	rm -f *.o *.kld *.ko
	rm -f @ machine
	rm -f ${CLEANFILES}
	rm -f .*.swp
	rm -f .*.swo
	rm -f /usr/src/contrib/libpcap/.*.swp

man:
	mv /usr/share/man/man4/em.4.gz /usr/share/man/man4/emSAVE.4.gz
	cp em.4 /usr/share/man/man4/

.include <bsd.kmod.mk>
