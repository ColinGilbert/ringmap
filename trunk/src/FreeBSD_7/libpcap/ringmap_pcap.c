#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <paths.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>
#include <kvm.h>
#include <nlist.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/proc.h>

#include <sys/mbuf.h>
#include <sys/linker.h>
#include <sys/errno.h>
#include <sys/_bus_dma.h>
#include <sys/_iovec.h>

#include <machine/bus.h>

#include "pcap.h"
#include "pcap-int.h"

#include "../em/ringmap.h"


/* Driver Kernel object file id */
// int if_em_ko_fileid = -1;

/* to_ms parameter of pcap */
// int fiveg_to_ms = 0;

/* File descriptor of fiveg_cdev */
int ringmap_cdev_fd = -1;

/***	F U N C T I O N S	***/
int check_module (const char *);
int fiveg_set_iface_promisc(const char *);
int init_mmapped_capturing(const char *device, pcap_t *);
void uninit_mmapped_capturing(pcap_t *);
int pcap_read_ringmap(pcap_t *, int , pcap_handler , u_char *);
int ringmap_stats(pcap_t *p, struct pcap_stat *ps);
void ringmap_enable_capturing();
void ringmap_disable_capturing();


/* 
 * Check if  module is loaded.
 * Get descripor of /dev/ringmap (open)
 *
 * The name of linker file: MOD_NAME (fiveg_da.h)
 * Return: 	fileid 	- 	of kld file on success. 
 * 			<0 		- 	otherwise 
 */
int
check_module (const char *device)
{
	int error = 0;
	kvm_t *kd;
	struct nlist nl[]={ {NULL}, {NULL}, };
	char dev_path[1024], cmd[1024];
	char errbuf[_POSIX2_LINE_MAX];

	/* Try to find our module */
	error = kldfind(MOD_NAME);
	if (error == -1){
		/* Module seems to be not loaded */
		printf("[%s] Module %s is not loaded\n", __func__, MOD_NAME);
		return(-1);
	} else {
		
		sprintf(cmd, "ls /dev/| grep %s > /dev/null",RINGMAP_DEVICE);
		if (system(cmd)){
			printf("[%s] ringmap driver is NOT loaded\n", __func__);
			return (-1);			
		}

		/* dev_path = "/dev/RINGMAP_DEVICE_em[0,1]" */
		sprintf(dev_path, "/dev/"RINGMAP_DEVICE"%s", device);

		/* Open fiveg_cdev device for communication with our driver */
		if ((ringmap_cdev_fd = open(dev_path, O_RDWR)) == -1) {
			printf("[%s] Error by opening %s \n", __func__, dev_path);
			perror("/dev/" RINGMAP_DEVICE);
			return(-2);
		}
	}

	printf(RINGMAP_PREFIX"ringmap-Driver %s is loaded!\n", MOD_NAME);

	return (error);
}

/*
 * set interface in promisc mode.
 *
 * Return : 
 * 		 0 	- 	success
 * 		<0	-	error
 */
int fiveg_set_iface_promisc(const char *iface)
{
	int error = 0, res;
	char cmd[1024];

#if (__RINGMAP_DEB)
	printf("[%s] Try to PROMISC interface %s\n", __func__, iface);
#endif

	/* check activity */
	sprintf(cmd, "ifconfig %s | egrep 'status.* no carrier'", iface);
	if (!system(cmd)){
		printf(ERR_PREFIX"%s is NOT active. PROMISC and capturing is not possible!\n",  iface);
		return (-1);
	}

	/* cmd = "ifconfig -a | egrep ....*/
	sprintf(cmd, "ifconfig -a | egrep '%s:.*flags.*,PROMISC'", iface);
	if (!system(cmd)){
		printf("%s is in PROMISC mode \n",  iface);
		return (error);
	}

	sprintf(cmd, "ifconfig %s monitor promisc up", iface);

	/* Set interface in promisc mode */
	res = system(cmd);
	if (res == -1){
		printf(ERR_PREFIX"[%s] Impossible to set promisc mode on %s\n", __func__, iface);
		error = res;
	} else {
		if (res == 127){
			printf(ERR_PREFIX"[%s] Shell execution Error\n", __func__);
			error = -1;
		}
	}

	return (error);
}

/********************************************************
 * Open  (/dev/fiveg_da) device to communicate with 
 * kernel. Do 'ioctl' to get number of descriptoren and 
 * 'readv' to get pointers to buffers for packet data. 
 * And then map buffers by calling mmap (/dev/mem, ...)
 * in space of our user process. 
 ********************************************************/
int
init_mmapped_capturing(const char *device, pcap_t *p)
{
	struct ring_slot sl; 
	int devmem_fd, i;
	long num_of_bytes;
	void *tmp_addr;
	struct ring tmp_ring;

	/* temp variable for save physical addresses (for mmap(/dev/mem)) */
	off_t memoffset = 0; 
	
	/* Physical addres of ring structure */
	bus_addr_t nic_statspp,	rspp;

	/* 
	 * iovec for readv system call to get physical addresses of structures 
	 * and buffers that we want to map in user space
	 */
	// struct iovec *iov;

	RINGMAP_FUNC_DEBUG(start);
	
	/* Open mem device for mmaping of kernel memory regions in space of our process */
	if ((devmem_fd = open("/dev/mem", O_RDWR)) == -1){
		perror("/dev/mem");
		return (-1); 
	}

	/* STOP CAPTURING */
	ringmap_disable_capturing();

#if (__RINGMAP_DEB)
	printf("[%s] Number of descriptors: %d \n", __func__, SLOTS_NUMBER);
#endif
	
	/* 
	 * Alloc memory for iovec: 
 	 * 						+1 ring_struct pointer
	 * 						+1 statistic structure 
	 */
	// iov = (struct iovec *)malloc(sizeof(struct iovec) * (1));
	
	/* prepare iovec to get physical addresses of ring pointers struct 	*/
	//iov[0].iov_base	= &rspp;
	//iov[0].iov_len	= sizeof(bus_addr_t);


	/* 
	 * Get from kern phys. addresses of mbufs, packet buffers, ring pointer struct 
	 * to map them later in space our process
	 */
	//num_of_bytes = readv(ringmap_cdev_fd, iov, 1);

//#if (__RINGMAP_DEB)
//	printf("[%s] bytes copied by readv: num_of_bytes = %d \n", __func__,  num_of_bytes);
//#endif

//	if (num_of_bytes < 0){
//		RINGMAP_ERROR("Reading the pointer to ring structure  from kernel failed!");
//		perror("/dev/" RINGMAP_DEVICE);
//		return -1;
//	}

//#if (__RINGMAP_DEB) 
//	printf("[%s] rspp = 0x%X\n", __func__, rspp);
//	printf("[%s] nic_statspp = 0x%X\n", __func__, nic_statspp);
//	printf("---\n");
//#endif 
	
	// memoffset = (off_t)rspp;
	if (ringmap_cdev_fd < 0){
		RINGMAP_ERROR(ringmap char device seems tgo be not open);
		return (-1);
	}
	tmp_addr = mmap(0, sizeof(struct ring), PROT_WRITE|PROT_READ, MAP_SHARED, ringmap_cdev_fd, 0);
	if (tmp_addr == MAP_FAILED){
		RINGMAP_ERROR("Mapping of Ring Pointers structure failed! Exit!");
		return -1;
	}
#if (__RINGMAP_DEB)
	printf("Virtual address of ring is %d \n", tmp_addr);
#endif 

	p->ring = (struct ring *)tmp_addr;
	if (p->ring->size == 0){
		RINGMAP_ERROR("Wrong size of ring buffer! Exit!");
		return -1;
	}
	
#if (__RINGMAP_DEB)
	printf("Ring Size = %d \n", p->ring->size);
#endif 

	exit(1);

	memoffset = (off_t)p->ring->hw_stats.phys;
	tmp_addr = mmap(0, sizeof(struct e1000_hw_stats), PROT_WRITE|PROT_READ, MAP_SHARED, devmem_fd, memoffset);
	if (tmp_addr == MAP_FAILED){
		RINGMAP_ERROR("Mapping of Statistics structure failed! Exit!");
		return -1;
	}
	p->nic_statistics = (struct e1000_hw_stats *)tmp_addr;

#if (__RINGMAP_DEB) 
	printf("[%s] descs number got from kern = %d\n", __func__, p->ring->size);
#endif 

	/* 
	 * Mapping mbufs from kern to userspace. 
	 * mbufs internal pointers contain kernel space addresses - 
	 * it means, we schould notice kernel space addresses of mbufs 
	 * to be able to get correct mbufs from mbufs chain 
	 */
	for (i = 0; i < SLOTS_NUMBER; i++){

		/* Map mbuf */
		memoffset = (off_t)p->ring->slot[i].mbuf.phys;
		tmp_addr = 
				mmap (		
					0, 						/*	System will choose the addrress */
					sizeof(struct mbuf),	/*	Size of mapped region = mbuf 	*/
					PROT_WRITE|PROT_READ,	/*	protection: write & read 		*/
					MAP_SHARED,				/*	shared maping					*/
					devmem_fd,				/*	device is /dev/mem				*/
					memoffset				/*	offset is physical addres 		*/
				);
		if (tmp_addr == MAP_FAILED){
			printf(ERR_PREFIX"[%s] Mapping of mbuf %d failed! Exit!\n", __func__, i);
			return -1;
		}
		p->ring->slot[i].mbuf.user = (vm_offset_t)tmp_addr;

		/* Map packet data */
		memoffset = (off_t)p->ring->slot[i].packet.phys;
		tmp_addr = 
				mmap (		
					0, 						/*	System will choose the addrress */
					MCLBYTES,				/*	Size of region = mbuf cluster	*/
					PROT_WRITE|PROT_READ,	/*	protection: write & read 		*/
					MAP_SHARED,				/*	shared maping					*/
					devmem_fd,				/*	device is /dev/mem				*/
					memoffset				/*	offset is physical addres 		*/
				);
		if (tmp_addr == MAP_FAILED){
			printf(ERR_PREFIX"[%s] Mapping of packet buffer %d failed! Exit!\n", __func__, i);
			return -1;
		}
		p->ring->slot[i].packet.user = (vm_offset_t)tmp_addr;
		
		/* Map descriptor structure */
		memoffset = (off_t)p->ring->slot[i].descriptor.phys;
		tmp_addr = 
				mmap (		
					0, 						/*	System will choose the addrress */
					16,						/*	Size of region = descr struct	*/
					PROT_WRITE|PROT_READ,	/*	protection: write & read 		*/
					MAP_SHARED,				/*	shared maping					*/
					devmem_fd,				/*	device is /dev/mem				*/
					memoffset				/*	offset is physical addres 		*/
				);
		if (tmp_addr == MAP_FAILED){
			printf(ERR_PREFIX"[%s] Mapping of descriptor structure %d failed! Exit!\n", __func__, i);
			return -1;
		}
		p->ring->slot[i].descriptor.user = (vm_offset_t)tmp_addr;


#if (__RINGMAP_DEB)
		printf("[%s] %d packet.user=0x%X, packet.phys=0x%X, packet.kern=0x%X\n ", 
				__func__, 
				i, 
				(unsigned int)p->ring->slot[i].packet.user, p->ring->slot[i].packet.phys, p->ring->slot[i].packet.kern);
#endif
	}

	/* Init our pcap variables */
	p->pkt_counter	=	0;
	p->buffer_fill	=	0;
	p->buffer_0		=	0;
	p->buffer_128	=	0;
	p->buffer_256	=	0;
	p->buffer_512	=	0;
	p->buffer_1024	=	0;
	p->buffer_2048 	=	0;
	p->buffer_4096	=	0;

	/* Disable Flow Control on the NIC */
	ioctl(ringmap_cdev_fd, IOCTL_DISABLE_FLOWCNTR); 

	/* Close memory device */
	if (close(devmem_fd) == -1){
		perror("close()");
	}

	/* START CAPTURING */
	ringmap_enable_capturing();

	return (0);
}

/*
 * Unmap buffers and free mem alocations 
 */
void 
uninit_mmapped_capturing(pcap_t *p)
{
	int tmp_res, i;

	ringmap_disable_capturing();

	for (i = 0; i < SLOTS_NUMBER; i++){
		tmp_res = munmap((void *)p->ring->slot[i].mbuf.user, sizeof(struct mbuf));
		if (tmp_res == -1)
			printf(ERR_PREFIX"[%s] Error by unmapping of mbuf %d\n", __func__, i);
	
		tmp_res = munmap((void *)p->ring->slot[i].packet.user, MCLBYTES);
		if (tmp_res == -1)
			printf(ERR_PREFIX"[%s] Error by unmapping of packet buffer %d\n", __func__, i);
	}

	if (p->ring != NULL){
		tmp_res = munmap(p->ring, sizeof(struct ring));
		if (tmp_res == -1)
			printf(ERR_PREFIX"[%s] Error by unmapping of ring pointer structure %d\n", __func__, i);
	}

	if (p->nic_statistics != NULL){
		tmp_res = munmap(p->nic_statistics, sizeof(struct e1000_hw_stats));
		if (tmp_res == -1)
			printf(ERR_PREFIX"[%s] Error by unmapping of statistic structure %d\n", __func__, i);
	}
	
	if (ringmap_cdev_fd >= 0){
		if ( close(ringmap_cdev_fd) == -1 )
			perror("close()");
	}
}

int
pcap_read_ringmap(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	unsigned int ws,  wait_flag = 1, tmp_dist; 
	struct mbuf *mb; 
	caddr_t datap; 
	struct pcap_pkthdr pkthdr;

	if (p->break_loop) {
		p->break_loop = 0;
		return (-2);
	}

again1: 
	
	// printf("user kern distance = %d\n", USER_TO_KERN_RING_DISTANCE(p->ring));

	if ( !(USER_TO_KERN_RING_DISTANCE(p->ring)) ){
		ioctl(ringmap_cdev_fd, IOCTL_SLEEP_WAIT);
		goto again1;
	}

	if (USER_TO_KERN_RING_DISTANCE(p->ring) < 128) 
		p->buffer_0 +=1;
	else
		if (USER_TO_KERN_RING_DISTANCE(p->ring) < 256) 
			p->buffer_128 +=1;
		else
			if (USER_TO_KERN_RING_DISTANCE(p->ring) < 512) 
				p->buffer_256 +=1;
			else
				if (USER_TO_KERN_RING_DISTANCE(p->ring) < 1024) 
					p->buffer_512 +=1;
				else
					if (USER_TO_KERN_RING_DISTANCE(p->ring) < 2048) 
						p->buffer_1024 +=1;

	if (cnt == -1)
		cnt = USER_TO_KERN_RING_DISTANCE(p->ring);

	for (ws = cnt; ws && (USER_TO_KERN_RING_DISTANCE(p->ring));) 
	{
		if (p->break_loop) {
			p->break_loop = 0;
			if (!(cnt - ws))
				return (-2);
			else 
				return (cnt - ws);
		}

		mb = (struct mbuf *)p->ring->slot[p->ring->userrp].mbuf.user;

		pkthdr.ts = p->ring->slot[p->ring->userrp].ts;
		pkthdr.caplen = pkthdr.len = mb->m_len;
		datap = (caddr_t)p->ring->slot[p->ring->userrp].packet.user;

		(*callback)(user, &pkthdr, datap);

		INC_USER_POINTER(p->ring);
		++p->pkt_counter;

		--ws;

	} 

	return (cnt - ws);
}

/*
 * Set hardware registers to enabling pkts receive and interrupts on NIC
 */
void 
ringmap_enable_capturing()
{
	if (ringmap_cdev_fd > 0){
		ioctl(ringmap_cdev_fd, IOCTL_ENABLE_RECEIVE);
	}else{
		printf("[%s] Error: Wrong descriptor of /dev/fiveg_cdev \n", __func__);
		printf("[%s] Error: Can't enable pkt receive \n", __func__);

		/* TODO: set return and check returned value */
		exit(1);
	}
}

/*
 * Set hardware registers to disabling pkts receive and interrupts on NIC
 */
void 
ringmap_disable_capturing()
{
	if (ringmap_cdev_fd > 0)
		ioctl(ringmap_cdev_fd, IOCTL_DISABLE_RECEIVE);
	else{
		printf("[%s] Error: Wrong descriptor of /dev/fiveg_cdev \n", __func__);
		printf("[%s] Error: Can't disable pkt receive \n", __func__);
		
		/* TODO: set return and check returned value */
		exit(1);
	}
}

/*
 * Prints Kern(Hardware)- and User(Software)-Pointers
 * Return Value: 
 * 	-1	-	Error
 */
int
print_ring_pointers(pcap_t *p)
{
	int err = 0;

	if (p->ring == NULL)
		return (-1);

	printf("Hardware Pointer (kern) = %d\n", p->ring->kernrp);
	printf("Software Pointer (user)	= %d\n", p->ring->userrp);

	return (err);
}

int 
print_ring_stats(pcap_t *p)
{
	int err = 0;

	if (p->ring == NULL)
		return (-1);

	printf("\nSYSTEM STATISTICS:\n");
	printf("----------------- \n");
	printf("Kernel waited for user capturing process:  %llu\n", p->ring->kern_wait_user);
	printf("Capturing User process waited for kernel:  %llu\n", p->ring->user_wait_kern);

	printf("Number of interrupts: %llu\n", p->ring->interrupts_counter);
	// printf("Packets per Interrupt: %llu\n", (((unsigned long long )p->pkt_counter) / p->ring->interrupts_counter));

	return (err);
}

/* 
 * Levels : 
 * 		0 	- only received and lose pkts
 * 		1 	- received pkts with pkts sizes and lose pkts
 * 		>2 	- full statistic
 */
void
print_hw_stats(int level, pcap_t *p)
{
	if (p->nic_statistics == NULL){
		return;
	}

	printf("VALUES FROM HARDWARE REGISTERS:\n");
	printf("------------------------------ \n");
	if (level >= 0){
		/* 
		 * Missed Packets Count:
		 * Counts the number of missed packets. Packets are missed when 
		 * the receive FIFO has insufficient space to store the incoming 
		 * packet. This can be caused because of too few buffers allocated, 
		 * or because there is insufficient bandwidth on the PCI bus. 
		 * Events setting this counter cause RXO, the Receiver Overrun 
		 * Interrupt, to be set. This register does not increment if receives are 
		 * not enabled.
		 */
		printf("Missed Packets = %lld\n",
			(long long)p->nic_statistics->mpc);
		
		/*
		 * Good Packets Received Count:
		 * Counts the number of good packets received of any legal length. This 
		 * counter does not include received flow control packets and only counts 
		 * packets that pass hardware filtering. 
		 *
		 * DOES NOT COUNT PACKETS COUNTED by the  Missed Packet Count.
		 */
		printf("Good Packets Rcvd = %lld\n",
	    	(long long)p->nic_statistics->gprc);
		
	}
	if (level >= 1){
	
		/* Will happen each time if RDH = RDT */
		printf("Receive No Memory Buffers = %lld\n",
			(long long)p->nic_statistics->rnbc);

		/* 
		 * Packets Received (64 Bytes) Count:
		 * Counts the number of good packets received that are exactly 64 bytes
		 * in length. Packets that are counted in the Missed Packet Count are not 
		 * counted in this counter. This counter does not count received flow control 
		 * packets.
		 */
		printf("Packets Received (64 Bytes) Count = %lld\n",
	    	(long long)p->nic_statistics->prc64);

		/* 
		 * Packets Received (65-127 Bytes) Count:
		 * Counts the number of good packets received that are 65-127 bytes in length. 
		 */
		printf("Packets Received (65-127 Bytes) Count = %lld\n",
	    	(long long)p->nic_statistics->prc127);

		/* 
		 * Packets Received (128-255 Bytes) Count
		 */
		printf("Packets Received (128-255 Bytes) Count = %lld\n",
	    	(long long)p->nic_statistics->prc255);

		/* 
		 * Packets Received (256-511 Bytes) Count
		 */
		printf("Packets Received (256-511 Bytes) Count = %lld\n",
	    	(long long)p->nic_statistics->prc511);

		/* 
		 * Packets Received (512-1023 Bytes) Count
		 */
		printf("Packets Received (512-1023 Bytes) Count = %lld\n",
	    	(long long)p->nic_statistics->prc1023);

		/* 
		 * Packets Received (1024 to Max Bytes) Count
		 */
		printf("Packets Received (1024 to Max Bytes) Count = %lld\n",
	    	(long long)p->nic_statistics->prc1522);

		/* 
		 * Total Packets Received:
		 * Counts the total number of all packets received. All packets received 
		 * are counted regardless of their length, whether they have errors, or whether 
		 * they are flow control packets.
		 */
		printf("Total Packets Received = %lld\n",
	    	(long long)p->nic_statistics->tpr);
	}

	if (level >= 2){

		printf("Sequence errors = %lld\n",
			(long long)p->nic_statistics->sec);

		printf("Defer count = %lld\n",
			(long long)p->nic_statistics->dc);


		printf("Receive Length Errors = %lld\n",
			((long long)p->nic_statistics->roc + (long long)p->nic_statistics->ruc));

		printf("Receive errors = %lld\n",
			(long long)p->nic_statistics->rxerrc);

		printf("Crc errors = %lld\n",
			(long long)p->nic_statistics->crcerrs);

		printf("Alignment errors = %lld\n",
			(long long)p->nic_statistics->algnerrc);

		printf("Collision/Carrier extension errors = %lld\n",
			(long long)p->nic_statistics->cexterr);
	}
}

int 
ringmap_stats(pcap_t *p, struct pcap_stat *ps)
{

	if ((ps == NULL) || (p->nic_statistics == NULL)){
		return (-1);
	}
	
	ps->ps_recv = p->nic_statistics->gprc;
	ps->ps_drop = p->nic_statistics->mpc;

	return (0);
}
