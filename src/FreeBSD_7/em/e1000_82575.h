/******************************************************************************

  Copyright (c) 2001-2008, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD: src/sys/dev/e1000/e1000_82575.h,v 1.1.2.2.2.1 2009/04/15 03:14:26 kensmith Exp $*/

#ifndef _E1000_82575_H_
#define _E1000_82575_H_

#define ID_LED_DEFAULT_82575_SERDES ((ID_LED_DEF1_DEF2 << 12) | \
                                     (ID_LED_DEF1_DEF2 <<  8) | \
                                     (ID_LED_DEF1_DEF2 <<  4) | \
                                     (ID_LED_OFF1_ON2))
/*
 * Receive Address Register Count
 * Number of high/low register pairs in the RAR.  The RAR (Receive Address
 * Registers) holds the directed and multicast addresses that we monitor.
 * These entries are also used for MAC-based filtering.
 */
/*
 * For 82576, there are an additional set of RARs that begin at an offset
 * separate from the first set of RARs.
 */
#define E1000_RAR_ENTRIES_82575   16
#define E1000_RAR_ENTRIES_82576   24

#ifdef E1000_BIT_FIELDS
struct e1000_adv_data_desc {
	u64 buffer_addr;    /* Address of the descriptor's data buffer */
	union {
		u32 data;
		struct {
			u32 datalen :16; /* Data buffer length */
			u32 rsvd    :4;
			u32 dtyp    :4;  /* Descriptor type */
			u32 dcmd    :8;  /* Descriptor command */
		} config;
	} lower;
	union {
		u32 data;
		struct {
			u32 status  :4;  /* Descriptor status */
			u32 idx     :4;
			u32 popts   :6;  /* Packet Options */
			u32 paylen  :18; /* Payload length */
		} options;
	} upper;
};

#define E1000_TXD_DTYP_ADV_C    0x2  /* Advanced Context Descriptor */
#define E1000_TXD_DTYP_ADV_D    0x3  /* Advanced Data Descriptor */
#define E1000_ADV_TXD_CMD_DEXT  0x20 /* Descriptor extension (0 = legacy) */
#define E1000_ADV_TUCMD_IPV4    0x2  /* IP Packet Type: 1=IPv4 */
#define E1000_ADV_TUCMD_IPV6    0x0  /* IP Packet Type: 0=IPv6 */
#define E1000_ADV_TUCMD_L4T_UDP 0x0  /* L4 Packet TYPE of UDP */
#define E1000_ADV_TUCMD_L4T_TCP 0x4  /* L4 Packet TYPE of TCP */
#define E1000_ADV_TUCMD_MKRREQ  0x10 /* Indicates markers are required */
#define E1000_ADV_DCMD_EOP      0x1  /* End of Packet */
#define E1000_ADV_DCMD_IFCS     0x2  /* Insert FCS (Ethernet CRC) */
#define E1000_ADV_DCMD_RS       0x8  /* Report Status */
#define E1000_ADV_DCMD_VLE      0x40 /* Add VLAN tag */
#define E1000_ADV_DCMD_TSE      0x80 /* TCP Seg enable */
/* Extended Device Control */
#define E1000_CTRL_EXT_NSICR    0x00000001 /* Disable Intr Clear all on read */

struct e1000_adv_context_desc {
	union {
		u32 ip_config;
		struct {
			u32 iplen    :9;
			u32 maclen   :7;
			u32 vlan_tag :16;
		} fields;
	} ip_setup;
	u32 seq_num;
	union {
		u64 l4_config;
		struct {
			u32 mkrloc :9;
			u32 tucmd  :11;
			u32 dtyp   :4;
			u32 adv    :8;
			u32 rsvd   :4;
			u32 idx    :4;
			u32 l4len  :8;
			u32 mss    :16;
		} fields;
	} l4_setup;
};
#endif

/* SRRCTL bit definitions */
#define E1000_SRRCTL_BSIZEPKT_SHIFT                     10 /* Shift _right_ */
#define E1000_SRRCTL_BSIZEHDRSIZE_MASK                  0x00000F00
#define E1000_SRRCTL_BSIZEHDRSIZE_SHIFT                 2  /* Shift _left_ */
#define E1000_SRRCTL_DESCTYPE_LEGACY                    0x00000000
#define E1000_SRRCTL_DESCTYPE_ADV_ONEBUF                0x02000000
#define E1000_SRRCTL_DESCTYPE_HDR_SPLIT                 0x04000000
#define E1000_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS          0x0A000000
#define E1000_SRRCTL_DESCTYPE_HDR_REPLICATION           0x06000000
#define E1000_SRRCTL_DESCTYPE_HDR_REPLICATION_LARGE_PKT 0x08000000
#define E1000_SRRCTL_DESCTYPE_MASK                      0x0E000000

#define E1000_SRRCTL_BSIZEPKT_MASK      0x0000007F
#define E1000_SRRCTL_BSIZEHDR_MASK      0x00003F00

#define E1000_TX_HEAD_WB_ENABLE   0x1
#define E1000_TX_SEQNUM_WB_ENABLE 0x2

#define E1000_MRQC_ENABLE_RSS_4Q            0x00000002
#define E1000_MRQC_ENABLE_VMDQ              0x00000003
#define E1000_MRQC_RSS_FIELD_IPV4_UDP       0x00400000
#define E1000_MRQC_RSS_FIELD_IPV6_UDP       0x00800000
#define E1000_MRQC_RSS_FIELD_IPV6_UDP_EX    0x01000000

#define E1000_VMRCTL_MIRROR_PORT_SHIFT      8
#define E1000_VMRCTL_MIRROR_DSTPORT_MASK    (7 << E1000_VMRCTL_MIRROR_PORT_SHIFT)
#define E1000_VMRCTL_POOL_MIRROR_ENABLE     (1 << 0)
#define E1000_VMRCTL_UPLINK_MIRROR_ENABLE   (1 << 1)
#define E1000_VMRCTL_DOWNLINK_MIRROR_ENABLE (1 << 2)

#define E1000_EICR_TX_QUEUE ( \
    E1000_EICR_TX_QUEUE0 |    \
    E1000_EICR_TX_QUEUE1 |    \
    E1000_EICR_TX_QUEUE2 |    \
    E1000_EICR_TX_QUEUE3)

#define E1000_EICR_RX_QUEUE ( \
    E1000_EICR_RX_QUEUE0 |    \
    E1000_EICR_RX_QUEUE1 |    \
    E1000_EICR_RX_QUEUE2 |    \
    E1000_EICR_RX_QUEUE3)

#define E1000_EIMS_RX_QUEUE E1000_EICR_RX_QUEUE
#define E1000_EIMS_TX_QUEUE E1000_EICR_TX_QUEUE

#define EIMS_ENABLE_MASK ( \
    E1000_EIMS_RX_QUEUE  | \
    E1000_EIMS_TX_QUEUE  | \
    E1000_EIMS_TCP_TIMER | \
    E1000_EIMS_OTHER)

/* Immediate Interrupt Rx (A.K.A. Low Latency Interrupt) */
#define E1000_IMIR_PORT_IM_EN     0x00010000  /* TCP port enable */
#define E1000_IMIR_PORT_BP        0x00020000  /* TCP port check bypass */
#define E1000_IMIREXT_SIZE_BP     0x00001000  /* Packet size bypass */
#define E1000_IMIREXT_CTRL_URG    0x00002000  /* Check URG bit in header */
#define E1000_IMIREXT_CTRL_ACK    0x00004000  /* Check ACK bit in header */
#define E1000_IMIREXT_CTRL_PSH    0x00008000  /* Check PSH bit in header */
#define E1000_IMIREXT_CTRL_RST    0x00010000  /* Check RST bit in header */
#define E1000_IMIREXT_CTRL_SYN    0x00020000  /* Check SYN bit in header */
#define E1000_IMIREXT_CTRL_FIN    0x00040000  /* Check FIN bit in header */
#define E1000_IMIREXT_CTRL_BP     0x00080000  /* Bypass check of ctrl bits */

/* Receive Descriptor - Advanced */
union e1000_adv_rx_desc {
	struct {
		u64 pkt_addr;             /* Packet buffer address */
		u64 hdr_addr;             /* Header buffer address */
	} read;
	struct {
		struct {
			union {
				u32 data;
				struct {
					u16 pkt_info; /* RSS type, Packet type */
					u16 hdr_info; /* Split Header,
				        	       * header buffer length */
				} hs_rss;
			} lo_dword;
			union {
				u32 rss;          /* RSS Hash */
				struct {
					u16 ip_id;    /* IP id */
					u16 csum;     /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			u32 status_error;     /* ext status/error */
			u16 length;           /* Packet length */
			u16 vlan;             /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

#define E1000_RXDADV_RSSTYPE_MASK        0x0000F000
#define E1000_RXDADV_RSSTYPE_SHIFT       12
#define E1000_RXDADV_HDRBUFLEN_MASK      0x7FE0
#define E1000_RXDADV_HDRBUFLEN_SHIFT     5
#define E1000_RXDADV_SPLITHEADER_EN      0x00001000
#define E1000_RXDADV_SPH                 0x8000
#define E1000_RXDADV_ERR_HBO             0x00800000

/* RSS Hash results */
#define E1000_RXDADV_RSSTYPE_NONE        0x00000000
#define E1000_RXDADV_RSSTYPE_IPV4_TCP    0x00000001
#define E1000_RXDADV_RSSTYPE_IPV4        0x00000002
#define E1000_RXDADV_RSSTYPE_IPV6_TCP    0x00000003
#define E1000_RXDADV_RSSTYPE_IPV6_EX     0x00000004
#define E1000_RXDADV_RSSTYPE_IPV6        0x00000005
#define E1000_RXDADV_RSSTYPE_IPV6_TCP_EX 0x00000006
#define E1000_RXDADV_RSSTYPE_IPV4_UDP    0x00000007
#define E1000_RXDADV_RSSTYPE_IPV6_UDP    0x00000008
#define E1000_RXDADV_RSSTYPE_IPV6_UDP_EX 0x00000009

/* RSS Packet Types as indicated in the receive descriptor */
#define E1000_RXDADV_PKTTYPE_NONE        0x00000000
#define E1000_RXDADV_PKTTYPE_IPV4        0x00000010 /* IPV4 hdr present */
#define E1000_RXDADV_PKTTYPE_IPV4_EX     0x00000020 /* IPV4 hdr + extensions */
#define E1000_RXDADV_PKTTYPE_IPV6        0x00000040 /* IPV6 hdr present */
#define E1000_RXDADV_PKTTYPE_IPV6_EX     0x00000080 /* IPV6 hdr + extensions */
#define E1000_RXDADV_PKTTYPE_TCP         0x00000100 /* TCP hdr present */
#define E1000_RXDADV_PKTTYPE_UDP         0x00000200 /* UDP hdr present */
#define E1000_RXDADV_PKTTYPE_SCTP        0x00000400 /* SCTP hdr present */
#define E1000_RXDADV_PKTTYPE_NFS         0x00000800 /* NFS hdr present */

#define E1000_RXDADV_PKTTYPE_IPSEC_ESP   0x00001000 /* IPSec ESP */
#define E1000_RXDADV_PKTTYPE_IPSEC_AH    0x00002000 /* IPSec AH */
#define E1000_RXDADV_PKTTYPE_LINKSEC     0x00004000 /* LinkSec Encap */
#define E1000_RXDADV_PKTTYPE_ETQF        0x00008000 /* PKTTYPE is ETQF index */
#define E1000_RXDADV_PKTTYPE_ETQF_MASK   0x00000070 /* ETQF has 8 indices */
#define E1000_RXDADV_PKTTYPE_ETQF_SHIFT  4          /* Right-shift 4 bits */

/* LinkSec results */
/* Security Processing bit Indication */
#define E1000_RXDADV_LNKSEC_STATUS_SECP         0x00020000
#define E1000_RXDADV_LNKSEC_ERROR_BIT_MASK      0x18000000
#define E1000_RXDADV_LNKSEC_ERROR_NO_SA_MATCH   0x08000000
#define E1000_RXDADV_LNKSEC_ERROR_REPLAY_ERROR  0x10000000
#define E1000_RXDADV_LNKSEC_ERROR_BAD_SIG       0x18000000

#define E1000_RXDADV_IPSEC_STATUS_SECP          0x00020000
#define E1000_RXDADV_IPSEC_ERROR_BIT_MASK       0x18000000
#define E1000_RXDADV_IPSEC_ERROR_INVALID_PROTOCOL       0x08000000
#define E1000_RXDADV_IPSEC_ERROR_INVALID_LENGTH         0x10000000
#define E1000_RXDADV_IPSEC_ERROR_AUTHENTICATION_FAILED  0x18000000

/* Transmit Descriptor - Advanced */
union e1000_adv_tx_desc {
	struct {
		u64 buffer_addr;    /* Address of descriptor's data buf */
		u32 cmd_type_len;
		u32 olinfo_status;
	} read;
	struct {
		u64 rsvd;       /* Reserved */
		u32 nxtseq_seed;
		u32 status;
	} wb;
};

/* Adv Transmit Descriptor Config Masks */
#define E1000_ADVTXD_DTYP_CTXT    0x00200000 /* Advanced Context Descriptor */
#define E1000_ADVTXD_DTYP_DATA    0x00300000 /* Advanced Data Descriptor */
#define E1000_ADVTXD_DCMD_EOP     0x01000000 /* End of Packet */
#define E1000_ADVTXD_DCMD_IFCS    0x02000000 /* Insert FCS (Ethernet CRC) */
#define E1000_ADVTXD_DCMD_RS      0x08000000 /* Report Status */
#define E1000_ADVTXD_DCMD_DDTYP_ISCSI  0x10000000 /* DDP hdr type or iSCSI */
#define E1000_ADVTXD_DCMD_DEXT    0x20000000 /* Descriptor extension (1=Adv) */
#define E1000_ADVTXD_DCMD_VLE     0x40000000 /* VLAN pkt enable */
#define E1000_ADVTXD_DCMD_TSE     0x80000000 /* TCP Seg enable */
#define E1000_ADVTXD_MAC_LINKSEC  0x00040000 /* Apply LinkSec on packet */
#define E1000_ADVTXD_MAC_TSTAMP   0x00080000 /* IEEE1588 Timestamp packet */
#define E1000_ADVTXD_STAT_SN_CRC  0x00000002 /* NXTSEQ/SEED present in WB */
#define E1000_ADVTXD_IDX_SHIFT    4  /* Adv desc Index shift */
#define E1000_ADVTXD_POPTS_ISCO_1ST  0x00000000 /* 1st TSO of iSCSI PDU */
#define E1000_ADVTXD_POPTS_ISCO_MDL  0x00000800 /* Middle TSO of iSCSI PDU */
#define E1000_ADVTXD_POPTS_ISCO_LAST 0x00001000 /* Last TSO of iSCSI PDU */
#define E1000_ADVTXD_POPTS_ISCO_FULL 0x00001800 /* 1st&Last TSO-full iSCSI PDU*/
#define E1000_ADVTXD_POPTS_IPSEC     0x00000400 /* IPSec offload request */
#define E1000_ADVTXD_PAYLEN_SHIFT    14 /* Adv desc PAYLEN shift */

/* Context descriptors */
struct e1000_adv_tx_context_desc {
	u32 vlan_macip_lens;
	u32 seqnum_seed;
	u32 type_tucmd_mlhl;
	u32 mss_l4len_idx;
};

#define E1000_ADVTXD_MACLEN_SHIFT    9  /* Adv ctxt desc mac len shift */
#define E1000_ADVTXD_VLAN_SHIFT     16  /* Adv ctxt vlan tag shift */
#define E1000_ADVTXD_TUCMD_IPV4    0x00000400  /* IP Packet Type: 1=IPv4 */
#define E1000_ADVTXD_TUCMD_IPV6    0x00000000  /* IP Packet Type: 0=IPv6 */
#define E1000_ADVTXD_TUCMD_L4T_UDP 0x00000000  /* L4 Packet TYPE of UDP */
#define E1000_ADVTXD_TUCMD_L4T_TCP 0x00000800  /* L4 Packet TYPE of TCP */
#define E1000_ADVTXD_TUCMD_IPSEC_TYPE_ESP    0x00002000 /* IPSec Type ESP */
/* IPSec Encrypt Enable for ESP */
#define E1000_ADVTXD_TUCMD_IPSEC_ENCRYPT_EN  0x00004000
#define E1000_ADVTXD_TUCMD_MKRREQ  0x00002000 /* Req requires Markers and CRC */
#define E1000_ADVTXD_L4LEN_SHIFT     8  /* Adv ctxt L4LEN shift */
#define E1000_ADVTXD_MSS_SHIFT      16  /* Adv ctxt MSS shift */
/* Adv ctxt IPSec SA IDX mask */
#define E1000_ADVTXD_IPSEC_SA_INDEX_MASK     0x000000FF
/* Adv ctxt IPSec ESP len mask */
#define E1000_ADVTXD_IPSEC_ESP_LEN_MASK      0x000000FF

/* Additional Transmit Descriptor Control definitions */
#define E1000_TXDCTL_QUEUE_ENABLE  0x02000000 /* Enable specific Tx Queue */
#define E1000_TXDCTL_SWFLSH        0x04000000 /* Tx Desc. write-back flushing */
/* Tx Queue Arbitration Priority 0=low, 1=high */
#define E1000_TXDCTL_PRIORITY      0x08000000

/* Additional Receive Descriptor Control definitions */
#define E1000_RXDCTL_QUEUE_ENABLE  0x02000000 /* Enable specific Rx Queue */
#define E1000_RXDCTL_SWFLSH        0x04000000 /* Rx Desc. write-back flushing */

/* Direct Cache Access (DCA) definitions */
#define E1000_DCA_CTRL_DCA_ENABLE  0x00000000 /* DCA Enable */
#define E1000_DCA_CTRL_DCA_DISABLE 0x00000001 /* DCA Disable */

#define E1000_DCA_CTRL_DCA_MODE_CB1 0x00 /* DCA Mode CB1 */
#define E1000_DCA_CTRL_DCA_MODE_CB2 0x02 /* DCA Mode CB2 */

#define E1000_DCA_RXCTRL_CPUID_MASK 0x0000001F /* Rx CPUID Mask */
#define E1000_DCA_RXCTRL_DESC_DCA_EN (1 << 5) /* DCA Rx Desc enable */
#define E1000_DCA_RXCTRL_HEAD_DCA_EN (1 << 6) /* DCA Rx Desc header enable */
#define E1000_DCA_RXCTRL_DATA_DCA_EN (1 << 7) /* DCA Rx Desc payload enable */

#define E1000_DCA_TXCTRL_CPUID_MASK 0x0000001F /* Tx CPUID Mask */
#define E1000_DCA_TXCTRL_DESC_DCA_EN (1 << 5) /* DCA Tx Desc enable */
#define E1000_DCA_TXCTRL_TX_WB_RO_EN (1 << 11) /* Tx Desc writeback RO bit */

#define E1000_DCA_TXCTRL_CPUID_MASK_82576 0xFF000000 /* Tx CPUID Mask */
#define E1000_DCA_RXCTRL_CPUID_MASK_82576 0xFF000000 /* Rx CPUID Mask */
#define E1000_DCA_TXCTRL_CPUID_SHIFT_82576 24 /* Tx CPUID */
#define E1000_DCA_RXCTRL_CPUID_SHIFT_82576 24 /* Rx CPUID */

/* Additional interrupt register bit definitions */
#define E1000_ICR_LSECPNS       0x00000020          /* PN threshold - server */
#define E1000_IMS_LSECPNS       E1000_ICR_LSECPNS   /* PN threshold - server */
#define E1000_ICS_LSECPNS       E1000_ICR_LSECPNS   /* PN threshold - server */

/* ETQF register bit definitions */
#define E1000_ETQF_FILTER_ENABLE   (1 << 26)
#define E1000_ETQF_IMM_INT         (1 << 29)
#define E1000_ETQF_1588            (1 << 30)
#define E1000_ETQF_QUEUE_ENABLE    (1 << 31)
/*
 * ETQF filter list: one static filter per filter consumer. This is
 *                   to avoid filter collisions later. Add new filters
 *                   here!!
 *
 * Current filters:
 *    EAPOL 802.1x (0x888e): Filter 0
 */
#define E1000_ETQF_FILTER_EAPOL          0

#define E1000_NVM_APME_82575          0x0400
#define MAX_NUM_VFS                   8

#define E1000_DTXSWC_MAC_SPOOF_MASK   0x000000FF /* Per VF MAC spoof control */
#define E1000_DTXSWC_VLAN_SPOOF_MASK  0x0000FF00 /* Per VF VLAN spoof control */
#define E1000_DTXSWC_LLE_MASK         0x00FF0000 /* Per VF Local LB enables */
#define E1000_DTXSWC_VMDQ_LOOPBACK_EN (1 << 31)  /* global VF LB enable */

/* Easy defines for setting default pool, would normally be left a zero */
#define E1000_VT_CTL_DEFAULT_POOL_SHIFT 7
#define E1000_VT_CTL_DEFAULT_POOL_MASK  (0x7 << E1000_VT_CTL_DEFAULT_POOL_SHIFT)

/* Other useful VMD_CTL register defines */
#define E1000_VT_CTL_IGNORE_MAC         (1 << 28)
#define E1000_VT_CTL_DISABLE_DEF_POOL   (1 << 29)
#define E1000_VT_CTL_VM_REPL_EN         (1 << 30)

/* Per VM Offload register setup */
#define E1000_VMOLR_LPE        0x00010000 /* Accept Long packet */
#define E1000_VMOLR_AUPE       0x01000000 /* Accept untagged packets */
#define E1000_VMOLR_BAM        0x08000000 /* Accept Broadcast packets */
#define E1000_VMOLR_MPME       0x10000000 /* Multicast promiscuous mode */
#define E1000_VMOLR_STRVLAN    0x40000000 /* Vlan stripping enable */

#define E1000_V2PMAILBOX_REQ   0x00000001 /* Request for PF Ready bit */
#define E1000_V2PMAILBOX_ACK   0x00000002 /* Ack PF message received */
#define E1000_V2PMAILBOX_VFU   0x00000004 /* VF owns the mailbox buffer */
#define E1000_V2PMAILBOX_PFU   0x00000008 /* PF owns the mailbox buffer */
#define E1000_V2PMAILBOX_PFSTS 0x00000010 /* PF wrote a message in the MB */
#define E1000_V2PMAILBOX_PFACK 0x00000020 /* PF ack the previous VF msg */
#define E1000_V2PMAILBOX_RSTI  0x00000040 /* PF has reset indication */

#define E1000_P2VMAILBOX_STS   0x00000001 /* Initiate message send to VF */
#define E1000_P2VMAILBOX_ACK   0x00000002 /* Ack message recv'd from VF */
#define E1000_P2VMAILBOX_VFU   0x00000004 /* VF owns the mailbox buffer */
#define E1000_P2VMAILBOX_PFU   0x00000008 /* PF owns the mailbox buffer */
#define E1000_P2VMAILBOX_RVFU  0x00000010 /* Reset VFU - used when VF stuck */

#define E1000_VFMAILBOX_SIZE   16 /* 16 32 bit words - 64 bytes */

/* If it's a E1000_VF_* msg then it originates in the VF and is sent to the
 * PF.  The reverse is TRUE if it is E1000_PF_*.
 * Message ACK's are the value or'd with 0xF0000000
 */
#define E1000_VT_MSGTYPE_ACK      0xF0000000  /* Messages below or'd with
                                               * this are the ACK */
#define E1000_VT_MSGTYPE_NACK     0xFF000000  /* Messages below or'd with
                                               * this are the NACK */
#define E1000_VT_MSGINFO_SHIFT    16
/* bits 23:16 are used for exra info for certain messages */
#define E1000_VT_MSGINFO_MASK     (0xFF << E1000_VT_MSGINFO_SHIFT)

#define E1000_VF_MSGTYPE_REQ_MAC  1 /* VF needs to know its MAC */
#define E1000_VF_MSGTYPE_VFLR     2 /* VF notifies VFLR to PF */
#define E1000_VF_SET_MULTICAST    3 /* VF requests PF to set MC addr */
#define E1000_VF_SET_VLAN         4 /* VF requests PF to set VLAN */

/* Add 100h to all PF msgs, leaves room for up to 255 discrete message types
 * from VF to PF - way more than we'll ever need */
#define E1000_PF_MSGTYPE_RESET    (1 + 0x100) /* PF notifies global reset
                                               * imminent to VF */
#define E1000_PF_MSGTYPE_LSC      (2 + 0x100) /* PF notifies VF of LSC... VF
                                               * will see extra msg info for
                                               * status */

#define E1000_PF_MSG_LSCDOWN      (1 << E1000_VT_MSGINFO_SHIFT)
#define E1000_PF_MSG_LSCUP        (2 << E1000_VT_MSGINFO_SHIFT)

#define ALL_QUEUES   0xFFFF

s32  e1000_send_mail_to_pf_vf(struct e1000_hw *hw, u32 *msg,
                              s16 size);
s32  e1000_receive_mail_from_pf_vf(struct e1000_hw *hw,
                                   u32 *msg, s16 size);
s32  e1000_send_mail_to_vf(struct e1000_hw *hw, u32 *msg,
                           u32 vf_number, s16 size);
s32  e1000_receive_mail_from_vf(struct e1000_hw *hw, u32 *msg,
                                u32 vf_number, s16 size);
void e1000_vmdq_loopback_enable_vf(struct e1000_hw *hw);
void e1000_vmdq_loopback_disable_vf(struct e1000_hw *hw);
void e1000_vmdq_replication_enable_vf(struct e1000_hw *hw, u32 enables);
void e1000_vmdq_replication_disable_vf(struct e1000_hw *hw);
void e1000_vmdq_enable_replication_mode_vf(struct e1000_hw *hw);
void e1000_vmdq_broadcast_replication_enable_vf(struct e1000_hw *hw,
						u32 enables);
void e1000_vmdq_multicast_replication_enable_vf(struct e1000_hw *hw,
						u32 enables);
void e1000_vmdq_broadcast_replication_disable_vf(struct e1000_hw *hw,
						u32 disables);
void e1000_vmdq_multicast_replication_disable_vf(struct e1000_hw *hw,
						u32 disables);
bool e1000_check_for_pf_ack_vf(struct e1000_hw *hw);

bool e1000_check_for_pf_mail_vf(struct e1000_hw *hw, u32*);


#endif
