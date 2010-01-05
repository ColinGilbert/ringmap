# Makefile for libpcap
# $FreeBSD: src/lib/libpcap/Makefile,v 1.39.2.1.6.1 2009/04/15 03:14:26 kensmith Exp $

SHLIBDIR?= /lib

.include <bsd.own.mk>

LIB=	pcap
SRCS=	grammar.y tokdefs.h version.h pcap-bpf.c \
	pcap.c inet.c fad-getad.c gencode.c optimize.c nametoaddr.c \
	etherent.c savefile.c bpf_filter.c bpf_image.c bpf_dump.c \
	scanner.l version.c
INCS=	pcap.h pcap-int.h pcap-namedb.h pcap-bpf.h
MAN=	pcap.3
CLEANFILES=tokdefs.h version.h version.c

YFLAGS+=-p pcapyy
LFLAGS+=-Ppcapyy
CFLAGS+=-DHAVE_CONFIG_H -Dyylval=pcapyylval -I${.CURDIR} -I.
CFLAGS+=-D_U_="__attribute__((unused))"
CFLAGS+=-DHAVE_SNPRINTF -DHAVE_VSNPRINTF



#####       FIVEG DA Block      #########
SRCS+=fiveg_da_pcap.c
INCS+=fiveg_da.h
CFLAGS+=-D__FIVEG_DA__
CFLAGS+=-lkvm

# Debugging output
CFLAGS+=-D__FIVEG_DA__DEB
#########################################



.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif
.if ${MK_PF} != "no"
CFLAGS+=-DHAVE_NET_PFVAR_H
.endif

SHLIB_MAJOR=5

#
# Magic to grab sources out of src/contrib
#
PCAP_DISTDIR?=${.CURDIR}
CFLAGS+=-I${PCAP_DISTDIR}
.PATH:	${PCAP_DISTDIR}
.PATH:	${PCAP_DISTDIR}/bpf/net

version.c: ${PCAP_DISTDIR}/VERSION
	@rm -f $@
	sed 's/.*/char pcap_version[] = "&";/' ${PCAP_DISTDIR}/VERSION > $@

version.h: ${PCAP_DISTDIR}/VERSION
	@rm -f $@
	sed 's/.*/char pcap_version_string[] = "libpcap version &";/' ${PCAP_DISTDIR}/VERSION > $@

tokdefs.h: grammar.h
	ln -sf grammar.h tokdefs.h

.include <bsd.lib.mk>
