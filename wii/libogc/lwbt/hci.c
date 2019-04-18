/*
 * Copyright (c) 2003 EISLAB, Lulea University of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwBT Bluetooth stack.
 *
 * Author: Conny Ohult <conny@sm.luth.se>
 *
 */

/*-----------------------------------------------------------------------------------*/
/* hci.c
 *
 * Implementation of the Host Controller Interface (HCI). A command interface to the
 * baseband controller and link manager, and gives access to hardware status and
 * control registers.
 *
 */
/*-----------------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>

#include "hci.h"
#include "l2cap.h"
#include "btmemr.h"
#include "btmemb.h"
#include "btpbuf.h"
#include "physbusif.h"

struct hci_pcb *hci_dev = NULL;
struct hci_link *hci_active_links = NULL;
struct hci_link *hci_tmp_link = NULL;
struct hci_link_key *hci_tmp_key = NULL;

MEMB(hci_pcbs,sizeof(struct hci_pcb),MEMB_NUM_HCI_PCB);
MEMB(hci_links,sizeof(struct hci_link),MEMB_NUM_HCI_LINK);
MEMB(hci_inq_results,sizeof(struct hci_inq_res),MEMB_NUM_HCI_INQ);
MEMB(hci_link_key_results,sizeof(struct hci_link_key),MEMB_NUM_HCI_LINK_KEY);

err_t hci_init(void)
{
	btmemr_init();
	btpbuf_init();

	btmemb_init(&hci_pcbs);
	btmemb_init(&hci_links);
	btmemb_init(&hci_inq_results);
	btmemb_init(&hci_link_key_results);

	if((hci_dev=btmemb_alloc(&hci_pcbs))==NULL) {
		ERROR("hci_init: Could not allocate memory for hci_dev\n");
		return ERR_MEM;
	}
	memset(hci_dev,0,sizeof(struct hci_pcb));

	hci_active_links = NULL;
	hci_tmp_link = NULL;

	return ERR_OK;
}

struct hci_link* hci_new(void)
{
	struct hci_link *link;

	link = btmemb_alloc(&hci_links);
	if(link==NULL) return NULL;

	memset(link,0,sizeof(struct hci_link));
	return link;
}

struct hci_link* hci_get_link(struct bd_addr *bdaddr)
{
	struct hci_link *link;

	for(link=hci_active_links;link!=NULL;link=link->next) {
		if(bd_addr_cmp(&(link->bdaddr),bdaddr)) break;
	}
	return link;
}

/*-----------------------------------------------------------------------------------*/
/*
 * hci_close():
 *
 * Close the link control block.
 */
/*-----------------------------------------------------------------------------------*/
err_t hci_close(struct hci_link *link)
{
	if(link->p != NULL) {
		btpbuf_free(link->p);
	}

	HCI_RMV(&(hci_active_links), link);
	btmemb_free(&hci_links, link);
	link = NULL;
	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/*
 * hci_reset_all():
 *
 * Closes all active link control blocks.
 */
/*-----------------------------------------------------------------------------------*/
void hci_reset_all(void)
{
	struct hci_link *link,*tlink;
	struct hci_inq_res *ires,*tires;
	struct hci_link_key *ikeys,*tikeys;

	for(link=hci_active_links;link!=NULL;) {
		tlink = link->next;
		hci_close(link);
		link = tlink;
	}
	hci_active_links = NULL;

	for(ires=hci_dev->ires;ires!=NULL;) {
		tires = ires->next;
		btmemb_free(&hci_inq_results,ires);
		ires = tires;
	}

	for(ikeys=hci_dev->keyres;ikeys!=NULL;) {
		tikeys = ikeys->next;
		btmemb_free(&hci_inq_results,ikeys);
		ikeys = tikeys;
	}
	btmemb_free(&hci_pcbs,hci_dev);

	hci_init();
}

void hci_arg(void *arg)
{
	hci_dev->cbarg = arg;
}

void hci_cmd_complete(err_t (*cmd_complete)(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result))
{
	hci_dev->cmd_complete = cmd_complete;
}

/*-----------------------------------------------------------------------------------*/
/*
 * hci_pin_req():
 *
 * Used to specify the function that should be called when HCI has received a
 * PIN code request event.
 */
/*-----------------------------------------------------------------------------------*/
void hci_pin_req(err_t (* pin_req)(void *arg, struct bd_addr *bdaddr))
{
	hci_dev->pin_req = pin_req;
}
/*-----------------------------------------------------------------------------------*/
/*
 * hci_link_key_req():
 *
 * Used to specify the function that should be called when HCI has received a
 * Link Key request event.
 */
/*-----------------------------------------------------------------------------------*/
void hci_link_key_req(err_t (* link_key_req)(void *arg, struct bd_addr *bdaddr))
{
	hci_dev->link_key_req = link_key_req;
}
/*-----------------------------------------------------------------------------------*/
/*
 * hci_link_key_not():
 *
 * Used to specify the function that should be called when HCI has received a
 * link key notification event.
 */
/*-----------------------------------------------------------------------------------*/
void hci_link_key_not(err_t (* link_key_not)(void *arg, struct bd_addr *bdaddr, u8_t *key))
{
	hci_dev->link_key_not = link_key_not;
}

/*-----------------------------------------------------------------------------------*/
/*
 * hci_connection_complete():
 *
 * Used to specify the function that should be called when HCI has received a
 * connection complete event.
 */
/*-----------------------------------------------------------------------------------*/
void hci_connection_complete(err_t (* conn_complete)(void *arg, struct bd_addr *bdaddr))
{
	hci_dev->conn_complete = conn_complete;
}

/*-----------------------------------------------------------------------------------*/
/*
 * hci_wlp_complete():
 *
 * Used to specify the function that should be called when HCI has received a
 * successful write link policy complete event.
 */
/*-----------------------------------------------------------------------------------*/
void  hci_wlp_complete(err_t (* wlp_complete)(void *arg, struct bd_addr *bdaddr))
{
	hci_dev->wlp_complete = wlp_complete;
}

void hci_conn_req(err_t (*conn_req)(void *arg,struct bd_addr *bdaddr,u8_t *cod,u8_t link_type))
{
	hci_dev->conn_req = conn_req;
}

err_t hci_reg_dev_info(struct bd_addr *bdaddr,u8_t *cod,u8_t psrm,u8_t psm,u16_t co)
{
	struct hci_inq_res *ires;

	if(hci_dev==NULL) return ERR_VAL;

	if((ires=btmemb_alloc(&hci_inq_results))!=NULL) {
		bd_addr_set(&(ires->bdaddr),bdaddr);
		memcpy(ires->cod,cod,3);
		ires->psrm = psrm;
		ires->psm = psm;
		ires->co = co;
		ires->next = NULL;

		HCI_REG(&(hci_dev->ires),ires);
		return ERR_OK;
	}
	return ERR_MEM;
}

struct pbuf* hci_cmd_ass(struct pbuf *p,u8_t ocf,u8_t ogf,u8_t len)
{
	((u8_t*)p->payload)[0] = HCI_COMMAND_DATA_PACKET; /* cmd packet type */
	((u8_t*)p->payload)[1] = (ocf&0xff); /* OCF & OGF */
	((u8_t*)p->payload)[2] = ((ocf>>8)|(ogf<<2));
	((u8_t*)p->payload)[3] = len-HCI_CMD_HDR_LEN-1; /* Param len = plen - cmd hdr - ptype */

	if(hci_dev->num_cmd>0) hci_dev->num_cmd--;
	return p;
}

err_t hci_reset(void)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_RESET_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_reset: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_RESET_OCF,HCI_HC_BB_OGF,HCI_RESET_PLEN);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_read_buffer_size(void)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_R_BUF_SIZE_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_read_buffer_size: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_R_BUF_SIZE_OCF,HCI_INFO_PARAM_OGF,HCI_R_BUF_SIZE_PLEN);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_read_bd_addr(void)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_R_BD_ADDR_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_read_bd_addr: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_R_BD_ADDR_OCF,HCI_INFO_PARAM_OGF,HCI_R_BD_ADDR_PLEN);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_read_local_version(void)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_R_LOC_VERS_SIZE_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_read_local_version: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_R_LOC_VERSION_OCF,HCI_INFO_PARAM_OGF,HCI_R_LOC_VERS_SIZE_PLEN);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_read_local_features(void)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_R_LOC_FEAT_SIZE_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_read_local_features: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_R_LOC_FEATURES_OCF,HCI_INFO_PARAM_OGF,HCI_R_LOC_FEAT_SIZE_PLEN);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_read_stored_link_key()
{
	struct pbuf *p = NULL;
	struct hci_link_key *tmpres;

	/* Free any previous link key result list */
	while(hci_dev->keyres != NULL) {
		tmpres = hci_dev->keyres;
		hci_dev->keyres = hci_dev->keyres->next;
		btmemb_free(&hci_link_key_results,tmpres);
	}

	if((p=btpbuf_alloc(PBUF_RAW,HCI_R_STORED_LINK_KEY_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_read_stored_link_keys: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_R_STORED_LINK_KEY_OCF,HCI_HC_BB_OGF,HCI_R_STORED_LINK_KEY_PLEN);

	memcpy((void*)((u8_t*)p->payload + 4),hci_dev->bdaddr.addr,6);
	((u8_t*)p->payload)[10] = 1;

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_set_event_filter(u8_t filter_type,u8_t filter_cond_type,u8_t *cond)
{
	u32 cond_len = 0;
	struct pbuf *p = NULL;

	switch(filter_type) {
		case 0x00:
			cond_len = 0x00;
			break;
		case 0x01:
			switch(filter_cond_type) {
				case 0x00:
					cond_len = 0x00;
					break;
				case 0x01:
					cond_len = 0x06;
					break;
				case 0x02:
					cond_len = 0x06;
					break;
				default:
					break;
			}
			break;
		case 0x02:
			switch(filter_cond_type) {
				case 0x00:
					cond_len = 0x01;
					break;
				case 0x01:
					cond_len = 0x07;
					break;
				case 0x02:
					cond_len = 0x07;
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}

	if((p=btpbuf_alloc(PBUF_RAW,HCI_SET_EV_FILTER_PLEN+cond_len,PBUF_RAM))==NULL) {
		ERROR("hci_set_event_filter: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_SET_EV_FILTER_OCF,HCI_HC_BB_OGF,HCI_SET_EV_FILTER_PLEN+cond_len);
	((u8_t*)p->payload)[4] = filter_type;
	((u8_t*)p->payload)[5] = filter_cond_type;
	if(cond_len>0) memcpy(p->payload+6,cond,cond_len);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_write_page_timeout(u16_t timeout)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_W_PAGE_TIMEOUT_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_set_write_page_timeout: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_W_PAGE_TIMEOUT_OCF,HCI_HC_BB_OGF,HCI_W_PAGE_TIMEOUT_PLEN);
	((u16_t*)p->payload)[2] = htole16(timeout);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_write_scan_enable(u8_t scan_enable)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_W_SCAN_EN_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_set_write_page_timeout: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_W_SCAN_EN_OCF,HCI_HC_BB_OGF,HCI_W_SCAN_EN_PLEN);
	((u8_t*)p->payload)[4] = scan_enable;

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_inquiry(u32_t lap,u8_t inq_len,u8_t num_resp,err_t (*inq_complete)(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result))
{
	struct pbuf *p = NULL;
	struct hci_inq_res *tmpres;

	/* Free any previous inquiry result list */
	while(hci_dev->ires != NULL) {
		tmpres = hci_dev->ires;
		hci_dev->ires = hci_dev->ires->next;
		btmemb_free(&hci_inq_results,tmpres);
	}

	hci_dev->inq_complete = inq_complete;
	if((p=btpbuf_alloc(PBUF_RAW,HCI_INQUIRY_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_inquiry: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_INQUIRY_OCF,HCI_LINK_CTRL_OGF,HCI_INQUIRY_PLEN);
	((u8_t*)p->payload)[4] = (lap&0xff);
	((u8_t*)p->payload)[5] = (lap>>8);
	((u8_t*)p->payload)[6] = (lap>>16);

	((u8_t*)p->payload)[7] = inq_len;
	((u8_t*)p->payload)[8] = num_resp;

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_periodic_inquiry(u32_t lap,u16_t min_period,u16_t max_period,u8_t inq_len,u8_t num_resp,err_t (*inq_complete)(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result))
{
	struct pbuf *p = NULL;
	struct hci_inq_res *tmpres;

	/* Free any previous inquiry result list */
	while(hci_dev->ires != NULL) {
		tmpres = hci_dev->ires;
		hci_dev->ires = hci_dev->ires->next;
		btmemb_free(&hci_inq_results,tmpres);
	}

	hci_dev->inq_complete = inq_complete;
	if((p=btpbuf_alloc(PBUF_RAW,HCI_PERIODIC_INQUIRY_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_periodic_inquiry: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_PERIODIC_INQUIRY_OCF,HCI_LINK_CTRL_OGF,HCI_PERIODIC_INQUIRY_PLEN);

	/* Assembling cmd prameters */
	((u16_t*)p->payload)[2] = htole16(max_period);
	((u16_t*)p->payload)[3] = htole16(min_period);
	((u8_t*)p->payload)[8] = (lap&0xff);
	((u8_t*)p->payload)[9] = (lap>>8);
	((u8_t*)p->payload)[10] = (lap>>16);

	((u8_t*)p->payload)[11] = inq_len;
	((u8_t*)p->payload)[12] = num_resp;

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_exit_periodic_inquiry()
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_EXIT_PERIODIC_INQUIRY_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_exit_periodic_inquiry: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_EXIT_PERIODIC_INQUIRY_OCF,HCI_LINK_CTRL_OGF,HCI_EXIT_PERIODIC_INQUIRY_PLEN);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_accecpt_conn_request(struct bd_addr *bdaddr,u8_t role)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_ACCEPT_CONN_REQ_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_exit_periodic_inquiry: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_ACCEPT_CONN_REQ_OCF,HCI_LINK_CTRL_OGF,HCI_ACCEPT_CONN_REQ_PLEN);

	/* Assembling cmd prameters */
	memcpy((void*)(((u8_t*)p->payload)+4),bdaddr,6);
	((u8_t*)p->payload)[10] = role;

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_set_event_mask(u64_t ev_mask)
{
	u64_t mask;
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_SET_EV_MASK_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_set_event_mask: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_SET_EV_MASK_OCF,HCI_HC_BB_OGF,HCI_SET_EV_MASK_PLEN);

	mask = htole64(ev_mask);
	memcpy(((u8_t*)p->payload)+4,&mask,8);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_write_local_name(u8_t *name,u8_t len)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_W_LOCAL_NAME_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_write_local_name: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_W_LOCAL_NAME_OCF,HCI_HC_BB_OGF,HCI_W_LOCAL_NAME_PLEN);
	/* Assembling cmd prameters */
	memcpy(((u8_t *)p->payload) + 4, name, len);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_write_pin_type(u8_t type)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_W_PIN_TYPE_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_write_local_name: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_W_PIN_TYPE_OCF,HCI_HC_BB_OGF,HCI_W_PIN_TYPE_PLEN);
	/* Assembling cmd prameters */
	((u8_t *)p->payload)[4] = type;

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_read_remote_name(struct bd_addr *bdaddr)
{
	u16_t clock_offset;
	struct pbuf *p = NULL;
	struct hci_inq_res *ires;
	u8_t page_scan_repetition_mode, page_scan_mode;

	for(ires=hci_dev->ires;ires!=NULL;ires=ires->next) {
		if(bd_addr_cmp(&(ires->bdaddr),bdaddr)) {
			page_scan_repetition_mode = ires->psrm;
			page_scan_mode = ires->psm;
			clock_offset = ires->co;
			break;
		}
	}

	if(ires==NULL) {
		page_scan_repetition_mode = 0x01;
		page_scan_mode = 0x00;
		clock_offset = 0x00;
	}

	if((p=btpbuf_alloc(PBUF_RAW,HCI_R_REMOTE_NAME_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_read_remote_name: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_R_REMOTE_NAME_OCF,HCI_LINK_CTRL_OGF,HCI_R_REMOTE_NAME_PLEN);
	/* Assembling cmd prameters */
	memcpy(((u8_t *)p->payload+4),bdaddr->addr,6);
	((u8_t*)p->payload)[10] = page_scan_repetition_mode;
	((u8_t*)p->payload)[11] = page_scan_mode;
	((u16_t*)p->payload)[6] = htole16(clock_offset);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;

}

err_t hci_write_inquiry_mode(u8_t mode)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_W_INQUIRY_MODE_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_write_inquiry_mode: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_W_INQUIRY_MODE_OCF,HCI_HC_BB_OGF,HCI_W_INQUIRY_MODE_PLEN);
	/* Assembling cmd prameters */
	((u8_t*)p->payload)[4] = mode;

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_write_page_scan_type(u8_t type)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_W_PAGE_SCAN_TYPE_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_write_inquiry_mode: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_W_PAGE_SCAN_TYPE_OCF,HCI_HC_BB_OGF,HCI_W_PAGE_SCAN_TYPE_PLEN);
	/* Assembling cmd prameters */
	((u8_t*)p->payload)[4] = type;

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_write_inquiry_scan_type(u8_t type)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_W_INQUIRY_SCAN_TYPE_PLEN,PBUF_RAM))==NULL) {
		ERROR("hci_write_inquiry_mode: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,HCI_W_INQUIRY_SCAN_TYPE_OCF,HCI_HC_BB_OGF,HCI_W_INQUIRY_SCAN_TYPE_PLEN);
	/* Assembling cmd prameters */
	((u8_t*)p->payload)[4] = type;

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_vendor_specific_command(u8_t ocf,u8_t ogf,void *data,u8_t len)
{
	struct pbuf *p = NULL;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_W_VENDOR_CMD_PLEN + len,PBUF_RAM))==NULL) {
		ERROR("hci_vendor_specific_patch: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p,ocf,ogf,HCI_W_VENDOR_CMD_PLEN + len);
	/* Assembling cmd prameters */
	memcpy(((u8_t*)p->payload + 4),data,len);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}
/*-----------------------------------------------------------------------------------*/
/* hci_sniff_mode():
 *
 * Sets an ACL connection to low power Sniff mode.
 */
/*-----------------------------------------------------------------------------------*/
err_t hci_sniff_mode(struct bd_addr *bdaddr, u16_t max_interval, u16_t min_interval, u16_t attempt, u16_t timeout)
{
	struct pbuf *p;
	struct hci_link *link;

	/* Check if an ACL connection exists */
	link = hci_get_link(bdaddr);

	if(link == NULL) {
		ERROR("hci_sniff_mode: ACL connection does not exist\n");
		return ERR_CONN;
	}

	if((p = btpbuf_alloc(PBUF_TRANSPORT, HCI_SNIFF_PLEN, PBUF_RAM)) == NULL) { /* Alloc len of packet */
		ERROR("hci_sniff_mode: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_SNIFF_MODE_OCF, HCI_LINK_POLICY_OGF, HCI_SNIFF_PLEN);
	/* Assembling cmd prameters */
	((u16_t *)p->payload)[2] = htole16(link->connhdl);
	((u16_t *)p->payload)[3] = htole16(max_interval);
	((u16_t *)p->payload)[4] = htole16(min_interval);
	((u16_t *)p->payload)[5] = htole16(attempt);
	((u16_t *)p->payload)[6] = htole16(timeout);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}
/*-----------------------------------------------------------------------------------*/
/* hci_write_link_policy_settings():
 *
 * Control the modes (park, sniff, hold) that an ACL connection can take.
 *
 */
/*-----------------------------------------------------------------------------------*/
err_t hci_write_link_policy_settings(struct bd_addr *bdaddr, u16_t link_policy)
{
	struct pbuf *p;
	struct hci_link *link;

	/* Check if an ACL connection exists */
	link = hci_get_link(bdaddr);

	if(link == NULL) {
		ERROR("hci_write_link_policy_settings: ACL connection does not exist\n");
		return ERR_CONN;
	}

	if( (p = btpbuf_alloc(PBUF_TRANSPORT, HCI_W_LINK_POLICY_PLEN, PBUF_RAM)) == NULL) { /* Alloc len of packet */
		ERROR("hci_write_link_policy_settings: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_W_LINK_POLICY_OCF, HCI_LINK_POLICY_OGF, HCI_W_LINK_POLICY_PLEN);

	/* Assembling cmd prameters */
	((u16_t *)p->payload)[2] = htole16(link->connhdl);
	((u16_t *)p->payload)[3] = htole16(link_policy);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);
	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_pin_code_request_reply():
 *
 * Used to reply to a PIN Code Request event from the Host Controller and specifies
 * the PIN code to use for a connection.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_pin_code_request_reply(struct bd_addr *bdaddr, u8_t pinlen, u8_t *pincode)
{
	struct pbuf *p;

	if((p = btpbuf_alloc(PBUF_RAW, HCI_PIN_CODE_REQ_REP_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_pin_code_request_reply: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Reset buffer content just to make sure */
	memset((u8_t *)p->payload, 0, HCI_PIN_CODE_REQ_REP_PLEN);

	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_PIN_CODE_REQ_REP, HCI_LINK_CTRL_OGF, HCI_PIN_CODE_REQ_REP_PLEN);
	/* Assembling cmd prameters */
	memcpy(((u8_t *)p->payload) + 4, bdaddr->addr, 6);
	((u8_t *)p->payload)[10] = pinlen;
	memcpy(((u8_t *)p->payload) + 11, pincode, pinlen);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_link_key_req_reply():
 *
 * Used to reply to a Link Key Code Request event from the Host Controller and specifies
 * the Link Key to use for a connection.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_link_key_req_reply(struct bd_addr *bdaddr, unsigned char *link_key)
{
        struct pbuf *p;
        if ((p = btpbuf_alloc(PBUF_RAW, HCI_LINK_KEY_REQ_REP_PLEN, PBUF_RAM)) == NULL) {
                ERROR("hci_link_key_req_reply: Could not allocate memory for pbuf\n");
                return ERR_MEM;
        }

        p = hci_cmd_ass(p, HCI_LINK_KEY_REQ_REP, HCI_LINK_CTRL_OGF, HCI_LINK_KEY_REQ_REP_PLEN);
        //copy bdaddr to offset 0x4
        memcpy(((u8_t *)p->payload)+4, bdaddr->addr, 6);
        //copy Link Key (16 bytes long) to offset 10 (0xA)
        memcpy(((u8_t *)p->payload)+10, link_key, 16);
        //send command
        physbusif_output(p,p->tot_len);
        btpbuf_free(p);

        return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_pin_code_request_neg_reply():
 *
 * Used to reply to a PIN Code Request event from the Host Controller when the Host
 * cannot specify a PIN code to use for a connection.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_pin_code_request_neg_reply(struct bd_addr *bdaddr)
{
	struct pbuf *p;

	if((p=btpbuf_alloc(PBUF_RAW,HCI_PIN_CODE_REQ_NEG_REP_PLEN,PBUF_RAM)) == NULL) {
		ERROR("hci_pin_code_request_neg_reply: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	p = hci_cmd_ass(p,HCI_PIN_CODE_REQ_NEG_REP,HCI_LINK_CTRL_OGF,HCI_PIN_CODE_REQ_NEG_REP_PLEN);
	memcpy(((u8_t *)p->payload)+4, bdaddr->addr, 6);

	physbusif_output(p,p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_link_key_req_neg_reply():
 *
 * Used to reply to a Link Key Request event from the Host Controller when the Host
 * cannot specify a Link Key to use for a connection.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_link_key_req_neg_reply(struct bd_addr *bdaddr)
{
        struct pbuf *p;

        if ((p = btpbuf_alloc(PBUF_RAW, HCI_LINK_KEY_REQ_REP_NEG_PLEN, PBUF_RAM)) == NULL) {
                ERROR("hci_link_key_req_neg_repl: Could not allocate memory for pbuf\n");
                return ERR_MEM;
        }

        p = hci_cmd_ass(p, HCI_LINK_KEY_REQ_REP_NEG, HCI_LINK_CTRL_OGF, HCI_LINK_KEY_REQ_REP_NEG_PLEN);
        memcpy(((u8_t *)p->payload)+4, bdaddr->addr, 6);

        physbusif_output(p,p->tot_len);
        btpbuf_free(p);

        return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_disconnect():
 *
 * Used to terminate an existing connection.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_disconnect(struct bd_addr *bdaddr, u8_t reason)
{
	struct pbuf *p;
	struct hci_link *link;

	link = hci_get_link(bdaddr);

	if(link == NULL) {
		ERROR("hci_disconnect: Connection does not exist\n");
		return ERR_CONN; /* Connection does not exist */
	}
	if((p = btpbuf_alloc(PBUF_RAW, HCI_DISCONN_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_disconnect: Could not allocate memory for pbuf\n");
		return ERR_MEM; /* Could not allocate memory for pbuf */
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_DISCONN_OCF, HCI_LINK_CTRL_OGF, HCI_DISCONN_PLEN);

	/* Assembling cmd prameters */
	((u16_t *)p->payload)[2] = htole16(link->connhdl);
	((u8_t *)p->payload)[6] = reason;

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_reject_connection_request():
 *
 * Used to decline a new incoming connection request.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_reject_connection_request(struct bd_addr *bdaddr, u8_t reason)
{
	struct pbuf *p;

	if((p = btpbuf_alloc(PBUF_RAW, HCI_REJECT_CONN_REQ_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_reject_connection_request: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_REJECT_CONN_REQ_OCF, HCI_LINK_CTRL_OGF, HCI_REJECT_CONN_REQ_PLEN);
	/* Assembling cmd prameters */
	memcpy(((u8_t *)p->payload) + 4, bdaddr->addr, 6);
	((u8_t *)p->payload)[10] = reason;

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_write_stored_link_key():
 *
 * Writes a link key to be stored in the Bluetooth host controller.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_write_stored_link_key(struct bd_addr *bdaddr, u8_t *link)
{
  struct pbuf *p;

	if((p = btpbuf_alloc(PBUF_RAW, HCI_WRITE_STORED_LINK_KEY_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_write_stored_link_key: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_WRITE_STORED_LINK_KEY, HCI_HC_BB_OGF, HCI_WRITE_STORED_LINK_KEY_PLEN);
	/* Assembling cmd prameters */
	((u8_t *)p->payload)[4] = 0x01;
	memcpy(((u8_t *)p->payload) + 5, bdaddr->addr, 6);
	memcpy(((u8_t *)p->payload) + 11, link, 16);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_write_cod():
 *
 * Write the value for the Class_of_Device parameter, which is used to indicate its
 * capabilities to other devices.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_write_cod(u8_t *cod)
{
	struct pbuf *p;

	if((p = btpbuf_alloc(PBUF_RAW, HCI_W_COD_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_write_cod: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_W_COD_OCF, HCI_HC_BB_OGF, HCI_W_COD_PLEN);
	/* Assembling cmd prameters */
	memcpy(((u8_t *)p->payload)+4, cod, 3);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

err_t hci_read_current_lap(void)
{
	struct pbuf *p;

	if((p = btpbuf_alloc(PBUF_RAW, HCI_R_CUR_IACLAP_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_read_current_lap: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_R_CUR_IACLAP_OCF, HCI_HC_BB_OGF, HCI_R_CUR_IACLAP_PLEN);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_set_hc_to_h_fc():
 *
 * Used by the Host to turn flow control on or off in the direction from the Host
 * Controller to the Host.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_set_hc_to_h_fc(void)
{
	struct pbuf *p;

	if((p = btpbuf_alloc(PBUF_RAW, HCI_SET_HC_TO_H_FC_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_set_hc_to_h_fc: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_SET_HC_TO_H_FC_OCF, HCI_HC_BB_OGF, HCI_SET_HC_TO_H_FC_PLEN);
	/* Assembling cmd prameters */
	((u8_t *)p->payload)[4] = 0x01; /* Flow control on for HCI ACL Data Packets and off for HCI
									 SCO Data Packets in direction from Host Controller to
				 Host */
	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_host_buffer_size():
 *
 * Used by the Host to notify the Host Controller about the maximum size of the data
 * portion of HCI ACL Data Packets sent from the Host Controller to the Host.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_host_buffer_size(void)
{
	struct pbuf *p;
	if((p = btpbuf_alloc(PBUF_RAW, HCI_H_BUF_SIZE_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_host_buffer_size: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_H_BUF_SIZE_OCF, HCI_HC_BB_OGF, HCI_H_BUF_SIZE_PLEN);
	((u16_t *)p->payload)[2] = htole16(HCI_HOST_ACL_MAX_LEN); /* Host ACL data packet maximum length */
	((u8_t *)p->payload)[6] = 255; /* Host SCO Data Packet Length */
	*((u16_t *)(((u8_t *)p->payload)+7)) = htole16(HCI_HOST_MAX_NUM_ACL); /* Host max total num ACL data packets */
	((u16_t *)p->payload)[4] = htole16(1); /* Host Total Num SCO Data Packets */
	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	hci_dev->host_num_acl = HCI_HOST_MAX_NUM_ACL;

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* hci_host_num_comp_packets():
 *
 * Used by the Host to indicate to the Host Controller the number of HCI Data Packets
 * that have been completed for each Connection Handle since the previous
 * Host_Number_Of_Completed_Packets command was sent to the Host Controller.
 */
 /*-----------------------------------------------------------------------------------*/
err_t hci_host_num_comp_packets(u16_t conhdl, u16_t num_complete)
{
	struct pbuf *p;

	if((p = btpbuf_alloc(PBUF_RAW, HCI_H_NUM_COMPL_PLEN, PBUF_RAM)) == NULL) {
		ERROR("hci_host_num_comp_packets: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}
	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_H_NUM_COMPL_OCF, HCI_HC_BB_OGF, HCI_H_NUM_COMPL_PLEN);
	((u8_t*)p->payload)[4] = 1;
	*(u16_t*)(p->payload+5) = htole16(conhdl);
	*(u16_t*)(p->payload+7) = htole16(num_complete); /* Number of completed acl packets */

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	hci_dev->host_num_acl += num_complete;

	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* lp_pdu_maxsize():
 *
 * Called by L2CAP to check the maxsize of the PDU. In this case it is the largest
 * ACL packet that the Host Controller can buffer.
 */
/*-----------------------------------------------------------------------------------*/
u16_t lp_pdu_maxsize()
{
	return hci_dev->acl_mtu;
}

/*-----------------------------------------------------------------------------------*/
/* lp_is_connected():
 *
 * Called by L2CAP to check if an active ACL connection exists for the specified
 * Bluetooth address.
 */
/*-----------------------------------------------------------------------------------*/
u8_t lp_is_connected(struct bd_addr *bdaddr)
{
	struct hci_link *link;

	link = hci_get_link(bdaddr);

	if(link == NULL) {
		return 0;
	}
	return 1;
}

/*-----------------------------------------------------------------------------------*/
/* lp_acl_write():
 *
 * Called by L2CAP to send data to the Host Controller that will be transfered over
 * the ACL link from there.
 */
/*-----------------------------------------------------------------------------------*/
err_t lp_acl_write(struct bd_addr *bdaddr,struct pbuf *p,u16_t len,u8_t pb)
{
	u16_t connhdlpbbc;
	struct hci_link *link;
	struct hci_acl_hdr *aclhdr;
	struct pbuf *q;

	link = hci_get_link(bdaddr);
	if(link==NULL) {
		ERROR("lp_acl_write: ACL connection does not exist\n");
		return ERR_CONN;
	}

	if(hci_dev->acl_max_pkt==0) {
		if(p != NULL) {
			/* Packet can be queued? */
			if(link->p != NULL) {
				LOG("lp_acl_write: Host buffer full. Dropped packet\n");
				return ERR_OK; /* Drop packet */
			} else {
				/* Copy PBUF_REF referenced payloads into PBUF_RAM */
				p = btpbuf_take(p);
				/* Remember pbuf to queue, if any */
				link->p = p;
				link->len = len;
				link->pb = pb;
				/* Pbufs are queued, increase the reference count */
				btpbuf_ref(p);
				LOG("lp_acl_write: Host queued packet %p\n", (void *)p);
			}
		}
	}

	if((q=btpbuf_alloc(PBUF_RAW,HCI_ACL_HDR_LEN+1,PBUF_RAM))==NULL) {
		ERROR("lp_acl_write: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	btpbuf_chain(q,p);
	((u8_t*)q->payload)[0] = HCI_ACL_DATA_PACKET;

	aclhdr = (void*)((u8_t*)q->payload+1);
	//aclhdr->connhdl_pb_bc = CONNPBBC(link->connhdl,pb,0);
	connhdlpbbc = link->connhdl; /* Received from connection complete event */
	connhdlpbbc |= (pb<<12); /* Packet boundary flag */
	connhdlpbbc &= 0x3FFF; /* Point-to-point */
	aclhdr->connhdl_pb_bc = htole16(connhdlpbbc);
	aclhdr->len = htole16(len);

	physbusif_output(q,(q->len+len));
	--hci_dev->acl_max_pkt;

	p = btpbuf_dechain(q);
	btpbuf_free(q);
	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* lp_write_flush_timeout():
 *
 * Called by L2CAP to set the flush timeout for the ACL link.
 */
/*-----------------------------------------------------------------------------------*/
err_t lp_write_flush_timeout(struct bd_addr *bdaddr, u16_t flushto)
{
	struct hci_link *link;
	struct pbuf *p;

	/* Check if an ACL connection exists */
	link = hci_get_link(bdaddr);

	if(link == NULL) {
		ERROR("lp_write_flush_timeout: ACL connection does not exist\n");
		return ERR_CONN;
	}

	if((p = btpbuf_alloc(PBUF_TRANSPORT, HCI_W_FLUSHTO_PLEN, PBUF_RAM)) == NULL) { /* Alloc len of packet */
		ERROR("lp_write_flush_timeout: Could not allocate memory for pbuf\n");
		return ERR_MEM;
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_W_FLUSHTO, HCI_HC_BB_OGF, HCI_W_FLUSHTO_PLEN);
	/* Assembling cmd prameters */
	((u16_t *)p->payload)[2] = htole16(link->connhdl);
	((u16_t *)p->payload)[3] = htole16(flushto);

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);
	return ERR_OK;
}

/*-----------------------------------------------------------------------------------*/
/* lp_connect_req():
 *
 * Called by L2CAP to cause the Link Manager to create a connection to the
 * Bluetooth device with the BD_ADDR specified by the command parameters.
 */
/*-----------------------------------------------------------------------------------*/
err_t lp_connect_req(struct bd_addr *bdaddr, u8_t allow_role_switch)
{
	u8_t page_scan_repetition_mode, page_scan_mode;
	u16_t clock_offset;
	struct pbuf *p;
	struct hci_link *link = hci_new();
	struct hci_inq_res *inqres;

	if(link == NULL) {
		ERROR("lp_connect_req: Could not allocate memory for link\n");
		return ERR_MEM; /* Could not allocate memory for link */
	}

	bd_addr_set(&(link->bdaddr), bdaddr);
	HCI_REG(&(hci_active_links), link);

	/* Check if module has been discovered in a recent inquiry */
	for(inqres = hci_dev->ires; inqres != NULL; inqres = inqres->next) {
		if(bd_addr_cmp(&inqres->bdaddr, bdaddr)) {
			page_scan_repetition_mode = inqres->psrm;
			page_scan_mode = inqres->psm;
			clock_offset = inqres->co;
			break;
		}
	}
	if(inqres == NULL) {
		/* No information on parameters from an inquiry. Using default values */
		page_scan_repetition_mode = 0x01; /* Assuming worst case: time between
											 successive page scans starting
											 <= 2.56s */
		page_scan_mode = 0x00; /* Assumes the device uses mandatory scanning, most
		devices use this. If no conn is established, try
		again w this parm set to optional page scanning */
		clock_offset = 0x00; /* If the device was not found in a recent inquiry
		this  information is irrelevant */
	}

	if((p = btpbuf_alloc(PBUF_RAW, HCI_CREATE_CONN_PLEN, PBUF_RAM)) == NULL) {
		ERROR("lp_connect_req: Could not allocate memory for pbuf\n");
		return ERR_MEM; /* Could not allocate memory for pbuf */
	}

	/* Assembling command packet */
	p = hci_cmd_ass(p, HCI_CREATE_CONN_OCF, HCI_LINK_CTRL_OGF, HCI_CREATE_CONN_PLEN);
	/* Assembling cmd prameters */
	memcpy(((u8_t *)p->payload)+4, bdaddr->addr, 6);
	((u16_t *)p->payload)[5] = htole16(hci_dev->pkt_type);
	((u8_t *)p->payload)[12] = page_scan_repetition_mode;
	((u8_t *)p->payload)[13] = page_scan_mode;
	((u16_t *)p->payload)[7] = htole16(clock_offset);
	((u8_t *)p->payload)[16] = allow_role_switch;

	physbusif_output(p, p->tot_len);
	btpbuf_free(p);

	return ERR_OK;
}

static void hci_cc_info_param(u8_t ocf,struct pbuf *p)
{
	struct bd_addr *bdaddr;

	switch(ocf) {
		case HCI_READ_LOCAL_VERSION:
			if(((u8_t*)p->payload)[0]==HCI_SUCCESS) {
				hci_dev->info.hci_version = *((u8_t*)p->payload + 1);
				hci_dev->info.hci_revision = le16toh(*(u16_t*)((u8_t*)p->payload + 2));
				hci_dev->info.lmp_version = *((u8_t*)p->payload + 4);
				hci_dev->info.manufacturer = le16toh(*(u16_t*)((u8_t*)p->payload + 5));
				hci_dev->info.lmp_subversion = le16toh(*(u16_t*)((u8_t*)p->payload + 7));
				LOG("hci_cc_info_param(HCI_READ_LOCAL_VERSION): hci_version = %02x, hci_revision = %04x, lmp_version = %02x, manufacturer = %04x, lmp_suversion = %04x\n",hci_dev->info.hci_version,hci_dev->info.hci_revision,hci_dev->info.lmp_version,hci_dev->info.manufacturer,hci_dev->info.lmp_subversion);
			}
			break;
		case HCI_READ_LOCAL_FEATURES:
			if(((u8_t*)p->payload)[0]==HCI_SUCCESS) {
				memcpy(hci_dev->features,(void*)((u8_t*)p->payload+1),sizeof(hci_dev->features));

				if(hci_dev->features[0]&LMP_3SLOT)
					hci_dev->pkt_type |= (HCI_DM3|HCI_DH3);
				if(hci_dev->features[0]&LMP_5SLOT)
					hci_dev->pkt_type |= (HCI_DM5|HCI_DH5);
				if(hci_dev->features[1]&LMP_HV2)
					hci_dev->pkt_type |= HCI_HV2;
				if(hci_dev->features[1]&LMP_HV3)
					hci_dev->pkt_type |= HCI_HV3;
				LOG("hci_cc_info_param(HCI_READ_LOCAL_FEATURES): %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",hci_dev->features[0],hci_dev->features[1],hci_dev->features[2],hci_dev->features[3],
																											   hci_dev->features[4],hci_dev->features[5],hci_dev->features[6],hci_dev->features[7]);
			}
			break;
		case HCI_READ_BUFFER_SIZE:
			if(((u8_t*)p->payload)[0]==HCI_SUCCESS) {
				hci_dev->acl_mtu = le16toh(*(u16_t*)(((u8_t*)p->payload)+1));
				hci_dev->sco_mtu = *((u8_t*)p->payload+3);
				hci_dev->acl_max_pkt = le16toh(*(u16_t*)(((u8_t*)p->payload)+4));
				hci_dev->sco_max_pkt = le16toh(*(u16_t*)(((u8_t*)p->payload)+5));
				LOG("hci_cc_info_param(HCI_READ_BUFFER_SIZE): acl_mt = %d, sco_mt = %d, acl_max_pkt = %d, sco_max_pkt = %d\n",hci_dev->acl_mtu,hci_dev->sco_mtu,hci_dev->acl_max_pkt,hci_dev->sco_max_pkt);
			}
			break;
		case HCI_READ_BD_ADDR:
			if(((u8_t*)p->payload)[0]==HCI_SUCCESS) {
				bdaddr = (void*)((u8_t*)p->payload+1);
				LOG("hci_cc_info_param(HCI_READ_BD_ADDR): %02x:%02x:%02x:%02x:%02x:%02x",bdaddr->addr[0],bdaddr->addr[1],bdaddr->addr[2],bdaddr->addr[3],bdaddr->addr[4],bdaddr->addr[5]);
				bd_addr_set(&(hci_dev->bdaddr),bdaddr);
			}
			break;
	}
}

static void hci_cc_host_ctrl(u8_t ocf,struct pbuf *p)
{
	u8_t *lap;
	u8_t i,resp_off;

	//printf("hci_cc_host_ctrl(%02x)\n",ocf);
	switch(ocf) {
		case HCI_SET_HC_TO_H_FC:
			if(((u8_t*)p->payload)[0]==HCI_SUCCESS) hci_dev->flow = 1;
			break;
		case HCI_READ_CUR_IACLAP:
			if(((u8_t*)p->payload)[0]==HCI_SUCCESS) {
				for(i=0;i<((u8_t*)p->payload)[1];i++) {
					resp_off = (i*3);
					lap = (void*)(((u8_t*)p->payload)+(2+resp_off));
					printf("lap = 00%02x%02x%02x\n",lap[2],lap[1],lap[0]);
				}
			}
			break;
	}
}

static void hci_cc_link_policy(u8_t ocf,struct pbuf *p)
{
	err_t ret;
	struct hci_link *link;

	(void)ret;

	switch(ocf) {
		case HCI_W_LINK_POLICY:
			if(((u8_t*)p->payload)[0]==HCI_SUCCESS) {
				for(link=hci_active_links;link!=NULL;link=link->next) {
					if(link->connhdl==le16toh(*((u16_t*)(((u8_t*)p->payload)+1)))) break;
				}
				if(link==NULL) {
					LOG("hci_cc_link_policy: Connection does not exist\n");
					break;
				}
				HCI_EVENT_WLP_COMPLETE(hci_dev,&link->bdaddr,ret);
			} else {
				LOG("Unsuccessful HCI_W_LINK_POLICY.\n");
			}
			break;
	}
}

static void hci_conn_request_evt(struct pbuf *p)
{
	u8_t *cod;
	u8_t link_type;
	err_t ret = ERR_OK;
	struct bd_addr *bdaddr;
	struct hci_link *link;

	LOG("hci_conn_request_evt()\n");
	bdaddr = (void*)((u8_t*)p->payload);
	cod = (((u8_t*)p->payload)+6);
	link_type = *(((u8_t*)p->payload)+9);

	HCI_EVENT_CONN_REQ(hci_dev,bdaddr,cod,link_type,ret);
	if(ret==ERR_OK) {
		link = hci_get_link(bdaddr);
		if(link==NULL) {
			if((link=hci_new())==NULL) {
				ERROR("hci_conn_request_evt: Could not allocate memory for link. Disconnect\n");
				return;
			}

			bd_addr_set(&(link->bdaddr),bdaddr);
			HCI_REG(&(hci_active_links),link);
		}
		hci_accecpt_conn_request(bdaddr,0x00);
	} else {
	}
}

static void hci_conn_complete_evt(struct pbuf *p)
{
	err_t ret;
	struct bd_addr *bdaddr;
	struct hci_link *link;

	(void)ret;

	bdaddr = (void*)(((u8_t*)p->payload)+3);
	link = hci_get_link(bdaddr);
	LOG("hci_conn_complete_evt(%p,%02x - %02x:%02x:%02x:%02x:%02x:%02x)\n",link,((u8_t*)p->payload)[0],bdaddr->addr[0],bdaddr->addr[1],bdaddr->addr[2],bdaddr->addr[3],bdaddr->addr[4],bdaddr->addr[5]);
	switch(((u8_t*)p->payload)[0]) {
		case HCI_SUCCESS:
			if(link==NULL) {
				if((link=hci_new())==NULL) {
					ERROR("hci_conn_complete_evt: Could not allocate memory for link. Disconnect\n");
					hci_disconnect(bdaddr, HCI_OTHER_END_TERMINATED_CONN_LOW_RESOURCES);
					lp_disconnect_ind(bdaddr,HCI_CONN_TERMINATED_BY_LOCAL_HOST);
					break;
				}
				bd_addr_set(&(link->bdaddr),bdaddr);
				link->connhdl = le16toh(*((u16_t*)(((u8_t*)p->payload)+1)));
				HCI_REG(&(hci_active_links),link);
				HCI_EVENT_CONN_COMPLETE(hci_dev,bdaddr,ret);
				lp_connect_ind(&(link->bdaddr));
			} else {
				link->connhdl = le16toh(*((u16_t*)(((u8_t*)p->payload)+1)));
				HCI_EVENT_CONN_COMPLETE(hci_dev,bdaddr,ret);
				lp_connect_cfm(&(link->bdaddr),((u8_t*)p->payload)[10],ERR_OK);
			}
			break;
		case HCI_PAGE_TIMEOUT:
			break;
		default:
			if(link!=NULL) {
				hci_close(link);
				lp_connect_cfm(bdaddr,((u8_t*)p->payload)[10],ERR_CONN);
			}
			break;
	}
}

static void hci_inquiry_result_evt(struct pbuf *p)
{
	u8_t num_resp;
	u32_t i,resp_off;
	struct bd_addr *bdaddr;
	struct hci_inq_res *ires;

	num_resp = ((u8_t*)p->payload)[0];
	//printf("hci_inquriy_result_evt(%d)\n",num_resp);
	for(i=0;i<num_resp && i<MEMB_NUM_HCI_INQ;i++) {
		resp_off = (i*14);
		bdaddr = (void*)(((u8_t*)p->payload)+(1+resp_off));
		if((ires=btmemb_alloc(&hci_inq_results))!=NULL) {
			bd_addr_set(&(ires->bdaddr),bdaddr);
			ires->psrm = ((u8_t*)p->payload)[7+resp_off];
			ires->psm = ((u8_t*)p->payload)[8+resp_off];
			memcpy(ires->cod,((u8_t*)p->payload)+10+resp_off,3);
			ires->co = le16toh(*((u16_t*)(((u8_t*)p->payload)+13+resp_off)));
			ires->next = NULL;

			HCI_REG(&(hci_dev->ires),ires);
		} else
			ERROR("hci_inquriy_result_evt: Could not allocate memory for inquiry result\n");
	}

}

static void hci_return_link_key_evt(struct pbuf *p)
{
	u8_t num_keys;
	u32_t i,resp_off;
	struct bd_addr *bdaddr;
	struct hci_link_key *keyres;

	num_keys = ((u8_t*)p->payload)[0];
	//printf("hci_return_link_key_evt(%d)\n",num_keys);
	for(i=0;i<num_keys && i<MEMB_NUM_HCI_LINK_KEY;i++) {
		resp_off = (i*22);
		bdaddr = (void*)(((u8_t*)p->payload)+1+resp_off);
		if((keyres=btmemb_alloc(&hci_link_key_results))!=NULL) {
			bd_addr_set(&(keyres->bdaddr),bdaddr);
			memcpy(keyres->key,((u8_t*)p->payload)+7+resp_off,16);
			keyres->next = NULL;

			//printf("link key evt: %02x:%02x:%02x:%02x:%02x:%02x\n",bdaddr->addr[0],bdaddr->addr[1],bdaddr->addr[2],bdaddr->addr[3],bdaddr->addr[4],bdaddr->addr[5]);
			HCI_REG(&(hci_dev->keyres),keyres);
		} else
			ERROR("hci_return_link_key_evt: Could not allocate memory for link key result\n");
	}

}

void hci_event_handler(struct pbuf *p)
{
	err_t ret;
	u8_t i,resp_off;
	u16_t ogf,ocf,opc;
	u16_t connhdl;
	struct pbuf *q;
	struct hci_link *link;
	struct bd_addr *bdaddr;
	struct hci_evt_hdr *evthdr;

	(void)ret;

	evthdr = p->payload;
	btpbuf_header(p,-HCI_EVENT_HDR_LEN);

	switch(evthdr->code) {
		case HCI_INQUIRY_COMPLETE:
			//printf("HCI_INQUIRY_COMPLETE\n");
			HCI_EVENT_INQ_COMPLETE(hci_dev,((u8_t*)p->payload)[0],ret);
			break;
		case HCI_INQUIRY_RESULT:
			hci_inquiry_result_evt(p);
			break;
		case HCI_CONNECTION_COMPLETE:
			hci_conn_complete_evt(p);
			break;
		case HCI_CONNECTION_REQUEST:
			hci_conn_request_evt(p);
			break;
		case HCI_DISCONNECTION_COMPLETE:
			switch(((u8_t*)p->payload)[0]) {
				case HCI_SUCCESS:
					for(link=hci_active_links;link!=NULL;link=link->next) {
						if(link->connhdl==le16toh(*((u16_t*)(((u8_t*)p->payload)+1)))) break;
					}
					if(link!=NULL) {
						lp_disconnect_ind(&(link->bdaddr),((u8_t*)p->payload)[3]);
						hci_close(link);
					}
					break;
				default:
					return;
			}
			break;
		case HCI_ENCRYPTION_CHANGE:
			break;
		case HCI_QOS_SETUP_COMPLETE:
			break;
		case HCI_COMMAND_COMPLETE:
			hci_dev->num_cmd += ((u8_t*)p->payload)[0];
			btpbuf_header(p,-1);

			opc = le16toh(((u16_t*)p->payload)[0]);
			ocf = (opc&0x03ff);
			ogf = (opc>>10);
			btpbuf_header(p,-2);

			switch(ogf) {
				case HCI_INFO_PARAM:
					hci_cc_info_param(ocf,p);
					break;
				case HCI_HOST_C_N_BB:
					hci_cc_host_ctrl(ocf,p);
					break;
				case HCI_LINK_POLICY:
					hci_cc_link_policy(ocf,p);
					break;
			}
			HCI_EVENT_CMD_COMPLETE(hci_dev,ogf,ocf,((u8_t*)p->payload)[0],ret);
			break;
		case HCI_COMMAND_STATUS:
			if(((u8_t*)p->payload)[0]!=HCI_SUCCESS) {
				btpbuf_header(p,-2);

				opc = le16toh(((u16_t*)p->payload)[0]);
				ocf = (opc&0x03ff);
				ogf = (opc>>10);
				btpbuf_header(p,-2);

				HCI_EVENT_CMD_COMPLETE(hci_dev,ogf,ocf,((u8_t*)p->payload)[0],ret);
				btpbuf_header(p,4);
			}
			hci_dev->num_cmd += ((u8_t*)p->payload)[1];
			break;
		case HCI_HARDWARE_ERROR:
			//TODO: IS THIS FATAL??
			break;
		case HCI_ROLE_CHANGE:
			break;
		case HCI_NBR_OF_COMPLETED_PACKETS:
			for(i=0;i<((u8_t *)p->payload)[0];i++) {
				resp_off = i*4;
				hci_dev->acl_max_pkt += le16toh(*((u16_t *)(((u8_t *)p->payload) + 3 + resp_off)));
				connhdl = le16toh(*((u16_t *)(((u8_t *)p->payload) + 1 + resp_off)));

				for(link = hci_active_links; link != NULL; link = link->next) {
					if(link->connhdl == connhdl) break;
				}

				q = link == NULL ? NULL : link->p;
				/* Queued packet present? */
				if (q != NULL) {
					/* NULL attached buffer immediately */
					link->p = NULL;
					/* Send the queued packet */
					lp_acl_write(&link->bdaddr, q, link->len, link->pb);
					/* Free the queued packet */
					btpbuf_free(q);
				}
			}
			break;
		case HCI_MODE_CHANGE:
			printf("HCI_MODE_CHANGE\n");
			break;
		case HCI_DATA_BUFFER_OVERFLOW:
			//TODO: IS THIS FATAL????
			break;
		case HCI_MAX_SLOTS_CHANGE:
			break;
		case HCI_RETURN_LINK_KEYS:
			hci_return_link_key_evt(p);
			break;
		case HCI_PIN_CODE_REQUEST:
			bdaddr = (void *)((u8_t *)p->payload); /* Get the Bluetooth address */
			HCI_EVENT_PIN_REQ(hci_dev, bdaddr, ret); /* Notify application. If event is not registered,
													send a negative reply */
			break;
		case HCI_LINK_KEY_REQUEST:
			bdaddr = (void *)((u8_t *)p->payload); /* Get the Bluetooth address */
			HCI_EVENT_LINK_KEY_REQ(hci_dev, bdaddr, ret);
			break;
		case HCI_LINK_KEY_NOTIFICATION:
			bdaddr = (void *)((u8_t *)p->payload); /* Get the Bluetooth address */

			HCI_EVENT_LINK_KEY_NOT(hci_dev, bdaddr, ((u8_t *)p->payload) + 6, ret); /* Notify application.*/
			break;
		default:
			LOG("hci_event_input: Undefined event code 0x%x\n", evthdr->code);
			break;
	}
}

void hci_acldata_handler(struct pbuf *p)
{
	struct hci_acl_hdr *aclhdr;
	struct hci_link *link;
	u16_t conhdl;

	aclhdr = p->payload;
	btpbuf_header(p, -HCI_ACL_HDR_LEN);

	conhdl = le16toh(aclhdr->connhdl_pb_bc) & 0x0FFF; /* Get the connection handle from the first
					   12 bits */
	if(hci_dev->flow) {
		//TODO: XXX??? DO WE SAVE NUMACL PACKETS COMPLETED IN LINKS LIST?? SHOULD WE CALL
		//hci_host_num_comp_packets from the main loop when no data has been received from the
		//serial port???
		--hci_dev->host_num_acl;
		if(hci_dev->host_num_acl == 0) {
			hci_host_num_comp_packets(conhdl, HCI_HOST_MAX_NUM_ACL);
			hci_dev->host_num_acl = HCI_HOST_MAX_NUM_ACL;
		}
	}

	for(link = hci_active_links; link != NULL; link = link->next) {
		if(link->connhdl == conhdl) {
			break;
		}
	}

	if(link != NULL) {
		if(le16toh(aclhdr->len)) {
			//LOG("hci_acl_input: Forward ACL packet to higher layer p->tot_len = %d\n", p->tot_len);
			l2cap_input(p, &(link->bdaddr));
		} else {
			btpbuf_free(p); /* If length of ACL packet is zero, we silently discard it */
		}
	} else {
		btpbuf_free(p); /* If no acitve ACL link was found, we silently discard the packet */
	}
}
