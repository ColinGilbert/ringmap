#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <paths.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <limits.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/linker.h>
#include <sys/errno.h>
#include <sys/ioccom.h>

#include "pcap.h"
#include "pcap-int.h"

int init_mmapped_capturing(const char *device, pcap_t *);
void uninit_mmapped_capturing(pcap_t *);
int pcap_read_ringmap(pcap_t *, int , pcap_handler , u_char *);
void ringmap_setfilter(struct bpf_program *, pcap_t *);

int 
set_ringmap_status(int val, pcap_t *p)
{
	/* If active status already set do nothing */
	if (p->rp.status == 0)
		p->rp.status = val;
	return (p->rp.status);
}

/* 
 * Open  (/dev/iface) device to communicate with 
 * kernel. Map buffers by calling mmap(/dev/mem, ...)
 * in space of our user process. 
 */
int
init_mmapped_capturing(const char *device, pcap_t *p)
{
	int devmem_fd, i, queue_num;
	void *tmp_addr;
	char dev_path[1024];
	off_t memoffset = 0; 
	vm_paddr_t ring;

	RINGMAP_FUNC_DEBUG(start);

	sprintf(dev_path, "/dev/%s", device);

	/* Open /dev/ device for communication with our driver */
	if ((p->rp.cdev_fd = open(dev_path, O_RDWR)) == -1) {
		printf("[%s] Error by opening %s \n", __func__, dev_path);
		perror("/dev/" RINGMAP_DEVICE);
		return (-1);
	}

	/* Set queue number we want to attach to our thread */
	queue_num = DEFAULT_QUEUE;

	/* Associate our thread with the queue_num */
	ioctl(p->rp.cdev_fd, IOCTL_ATTACH_RING, &queue_num);

	/* 
	 * Open mem device for mmaping of kernel memory regions into space of our
	 * process 
	 */
	if ((devmem_fd = open("/dev/mem", O_RDWR)) == -1) {
		perror("/dev/mem");
		return (-1); 
	}

	/**
	 ** Get the physical address of the ring
	 **/
	if (read(p->rp.cdev_fd, &ring, sizeof(vm_paddr_t)) == -1) {
		RINGMAP_ERROR(Can not get phys addr of ring!);
		return (-1);
	}


#if (__RINGMAP_DEB)
	printf("[%s] Phys addr of ring 0x%X\n", __func__, ring);
#endif

	/* Map ring into our address space */
	tmp_addr = mmap(0,	 		/* Kernel gives us the address */
		sizeof(struct ring), 	/* Number of bytes we are mapping */
		PROT_WRITE|PROT_READ,	/* We want both read and write */
		MAP_SHARED, 			/* Changes shoud be visible in kernel */
		devmem_fd, 				/* /dev/mem device */
		ring);					/* offset = phys addr of ring */
	if (tmp_addr == MAP_FAILED) {
		RINGMAP_ERROR("Mapping of Ring Pointers structure failed! Exit!");
		return (-1);
	}

	p->rp.ring = (struct ring *)tmp_addr;
	if (p->rp.ring->size != SLOTS_NUMBER) {
		RINGMAP_ERROR("Wrong size of ring buffer!");
		return (-1);
	}
	
#if (__RINGMAP_DEB)
	printf("Virtual address of ring is 0x%X\n", p->rp.ring);
	printf("Ring Size = %d \n", p->rp.ring->size);

	for (i=0 ;i < SLOTS_NUMBER; i++) {
		PACKET_ADDR_DEB(p->rp.ring, i);
	}

	PRINT_RING_PTRS(p->rp.ring);
#endif 

	/* 
	 * Mapping mbufs and packet buffers from kern into userspace. 
	 */
	for (i = 0; i < SLOTS_NUMBER; i++) {
		/* Map mbuf */
		memoffset = (off_t)p->rp.ring->slot[i].mbuf.phys;
		tmp_addr = mmap(0,	 		/*	System will choose the addrress */
			sizeof(struct mbuf),	/*	Size of mapped region (mbuf) 	*/
			PROT_WRITE|PROT_READ,	/*	protection: write & read 		*/
			MAP_SHARED,				/*	shared maping					*/
			devmem_fd,				/*	device is /dev/mem				*/
			memoffset				/*	offset is physical addres 		*/
						);
		if (tmp_addr == MAP_FAILED) {
			printf(ERR_PREFIX"[%s] Mapping of mbuf %d failed!\n", 
								__func__, i);
			return (-1);
		}
		U_MBUF(p->rp.ring, i) = (vm_offset_t)tmp_addr;

		/* Map packet data */
		memoffset = (off_t)p->rp.ring->slot[i].packet.phys;
		tmp_addr = 
			mmap(	0, 
					MCLBYTES, 
					PROT_WRITE|PROT_READ, 
					MAP_SHARED,				
					devmem_fd, 
					memoffset);
		if (tmp_addr == MAP_FAILED) {
			printf(ERR_PREFIX"[%s] Mapping of packets buffer %d failed!\n", 
					__func__, i);
			return (-1);
		}
		U_PACKET(p->rp.ring, i) = (vm_offset_t)tmp_addr;
		
		PACKET_ADDR_DEB(p->rp.ring, i);
	}

	/* Close memory device */
	if (close(devmem_fd) == -1) {
		perror("close()");
	}

	RINGMAP_FUNC_DEBUG(end);

	return (0);
}


/*
 * Unmap buffers and free mem alocations 
 */
void 
uninit_mmapped_capturing(pcap_t *p)
{
	int tmp_res, i;

	RINGMAP_FUNC_DEBUG(start);

	if (p == NULL) {
		RINGMAP_FUNC_DEBUG(NULL pointer to pcap structure);
		goto out;
	}

	if (p->rp.ring == NULL) {
		RINGMAP_FUNC_DEBUG(NULL pointer to ringstructure);
		goto out;
	}

	p->rp.status = 0;

	/* Unmap slots */
	for (i = 0; i < SLOTS_NUMBER; i++) {
		tmp_res = munmap((void *)U_MBUF(p->rp.ring, i), 
							sizeof(struct mbuf));

		if (tmp_res == -1)
			printf(ERR_PREFIX"[%s] Error by unmapping of mbuf %d\n", 
								__func__, i);
	
		tmp_res = munmap((void *)U_PACKET(p->rp.ring, i), MCLBYTES);
		if (tmp_res == -1)
			printf(ERR_PREFIX"[%s] Error by unmapping of packet buffer %d\n", 
					__func__, i);
	}

	/* Unmap ring */
	tmp_res = munmap(p->rp.ring, sizeof(struct ring));
	if (tmp_res == -1) {
		RINGMAP_ERROR(Unmaping the ring pointer);
	}

out:	
	if (p->rp.cdev_fd >= 0) {
		if (close(p->rp.cdev_fd) == -1)
			perror("close()");
	}

	RINGMAP_FUNC_DEBUG(end);
}

void
ringmap_setfilter(struct bpf_program *fp, pcap_t *p)
{
	if (ioctl(p->rp.cdev_fd, IOCTL_SETFILTER, (caddr_t)fp) == 0) {
		RINGMAP_FUNC_DEBUG(Filter is set);
	} else {
		RINGMAP_WARN(Filter is not set!);
	}
}

int
pcap_read_ringmap(pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	unsigned int ws,  wait_flag = 1, tmp_dist; 
	unsigned int curr_slot;
	int err_sleep, slen;
	struct mbuf *mb; 
	caddr_t datap; 
	struct pcap_pkthdr pkthdr;
	struct ring *ring;


	RINGMAP_FUNC_DEBUG(start);

	if (p->break_loop) {
		p->break_loop = 0;
		return (-2);
	}

	if (p->rp.ring == NULL) {
		RINGMAP_ERROR(Ring is not allocated);
		exit (1);
	}

	ring = p->rp.ring;

	/* stay here while the ring is empty */
	while (RING_IS_EMPTY(ring)) {

		RING_PTRS_DEB(ring);
		RINGMAP_FUNC_DEBUG(Ring is empty. Sleep...);

		/* Sleep and wait for new incoming packets */
		err_sleep = ioctl(p->rp.cdev_fd, IOCTL_SLEEP_WAIT);

		/* catching signals */
		if (err_sleep) {
			if ((errno == EINTR) || (errno == EINVAL)) {
				pcap_close(p);
				exit(0);
			}
		}
	}

	/* Ok, if we are here the ring shouldn't be empty, let's capture */
	RINGMAP_FUNC_DEBUG(Ring is NOT empty:);
	RING_PTRS_DEB(ring);

	if ((cnt == -1) || (cnt == 0))
		cnt = SW_TAIL_TO_HEAD_DIST(p->rp.ring);

	for (ws = cnt; ((ws) && (RING_NOT_EMPTY(p->rp.ring)));) {
		if (p->break_loop) {
			p->break_loop = 0;
			if (!(cnt - ws))
				return (-2);
			else 
				return (cnt - ws);
		}

		/* Slot we want to check */
		curr_slot = R_MODULO(SW_TAIL(ring) + 1);

		/* 
		 * ringmap-Driver tell us whether the slot contains 
		 * a good packet 
		 */
		if (ring->slot[curr_slot].is_ok == 0) {
#if (__RINGMAP_DEB)
			printf("Slot %d was not accepted by driver!\n", curr_slot);
#endif 
			goto out;
		}

		mb = (struct mbuf *)U_MBUF(ring, curr_slot);
		datap = (caddr_t)U_PACKET(ring, curr_slot);
		pkthdr.caplen = pkthdr.len = mb->m_len;
		pkthdr.ts = ring->slot[curr_slot].ts;

		/* Packet filtering */
		if (p->md.use_bpf) {
			/* Filtered in Kernel */
			if (ring->slot[curr_slot].filtered == 0) {
#if (__RINGMAP_DEB)
				printf("Slot %d was not filtered by kernel bpf!\n", curr_slot);
#endif 
				goto out;
			}
		} else {
			/* Filtering in user-space */
		    slen = bpf_filter(p->fcode.bf_insns, datap, mb->m_len, mb->m_len);
			if (!slen)
				goto out;
		}

		ring->pkt_counter++;
		--ws;

		PKT_BYTES_DEB(datap, curr_slot);	

		/* callback function */
		(*callback)(user, &pkthdr, datap);

		PRINT_SLOT_DEB(ring, curr_slot);

out:
		INCR_TAIL(ring);

		ring->slot[curr_slot].filtered = 0;
		ring->slot[curr_slot].is_ok = 0;
	} 

	RINGMAP_FUNC_DEBUG(end);

	return (cnt - ws);
}


/*
 * Prints HEAD (kern) and TAIL (user) pointers
 */
int
print_ring_pointers(pcap_t *p)
{
	int err = 0;

	if (p->rp.ring == NULL)
		return (-1);

	printf("HEAD Pointer (kern) = %d\n", SW_HEAD(p->rp.ring));
	printf("TAIL Pointer (user)	= %d\n", SW_TAIL(p->rp.ring));

	return (err);
}


int 
print_ring_stats(pcap_t *p)
{
	int err = 0;

	if (p->rp.ring == NULL) {
		RINGMAP_ERROR(Ring is not allocated!);

		return (-1);
	}

	printf("Ring-Full  counter:  %llu\n", p->rp.ring->kern_wait_user);
	printf("Ring-Empty counter:  %llu\n", p->rp.ring->user_wait_kern);
	printf("Interrupt counter:  %llu\n", p->rp.ring->intr_num);

	return (err);
}
