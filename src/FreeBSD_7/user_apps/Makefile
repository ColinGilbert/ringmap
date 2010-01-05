# ringmap - for capturing traffic only if Ring Map driver is loaded
# bpfmap  - capturing traffic through BPF device.
# ..._filecopy - save captured trafic in file
# ..._memcopy  - do memcopy for each received packet


SRC=fiveg_pcap.c

ringmap:	${SRC}
	cc -lpcap -DRING_MAP -D__FIVEG_DA__ -o ringmap ${SRC}

ringmap_filecopy:	${SRC}
	cc -lpcap -DRING_MAP -DFILE_COPY -o ringmap_filecopy ${SRC}

ringmap_memcopy:	${SRC}
	cc -lpcap -lkvm -DRING_MAP -DMEM_COPY -o ringmap_memcopy ${SRC}

generic:	${SRC}
	cc -lpcap -DBPF_MAP -o generic ${SRC}

generic_filecopy:	${SRC}
	cc -lpcap -DBPF_MAP -DFILE_COPY -o generic_filecopy ${SRC}

generic_memcopy:	${SRC}
	cc -lpcap -DBPF_MAP -DMEM_COPY -o generic_memcopy ${SRC}
