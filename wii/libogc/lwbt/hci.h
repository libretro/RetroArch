#ifndef __HCI_H__
#define __HCI_H__

#include "bt.h"
#include "bd_addr.h"

/* HCI packet indicators */
#define HCI_COMMAND_DATA_PACKET			0x01
#define HCI_ACL_DATA_PACKET				0x02
#define HCI_SCO_DATA_PACKET				0x03
#define HCI_EVENT_PACKET				0x04
#define HCI_VENDOR_PACKET				0xff

/* HCI packet types */
#define HCI_DM1							0x0008
#define HCI_DM3							0x0400
#define HCI_DM5							0x4000
#define HCI_DH1							0x0010
#define HCI_DH3							0x0800
#define HCI_DH5							0x8000

#define HCI_HV1							0x0020
#define HCI_HV2							0x0040
#define HCI_HV3							0x0080

#define SCO_PTYPE_MASK					(HCI_HV1 | HCI_HV2 | HCI_HV3)
#define ACL_PTYPE_MASK					(~SCO_PTYPE_MASK)

#define HCI_EVENT_HDR_LEN				2
#define HCI_ACL_HDR_LEN					4
#define HCI_SCO_HDR_LEN					3
#define HCI_CMD_HDR_LEN					3

/* LMP features */
#define LMP_3SLOT						0x01
#define LMP_5SLOT						0x02
#define LMP_ENCRYPT						0x04
#define LMP_SOFFSET						0x08
#define LMP_TACCURACY					0x10
#define LMP_RSWITCH						0x20
#define LMP_HOLD						0x40
#define LMP_SNIFF						0x80

#define LMP_PARK						0x01
#define LMP_RSSI						0x02
#define LMP_QUALITY						0x04
#define LMP_SCO							0x08
#define LMP_HV2							0x10
#define LMP_HV3							0x20
#define LMP_ULAW						0x40
#define LMP_ALAW						0x80

#define LMP_CVSD						0x01
#define LMP_PSCHEME						0x02
#define LMP_PCONTROL					0x04

/* Opcode Group Field (OGF) values */
#define HCI_LINK_CONTROL				0x01   /* Link Control Commands */
#define HCI_LINK_POLICY 0x02    /* Link Policy Commands */
#define HCI_HOST_C_N_BB 0x03    /* Host Controller & Baseband Commands */
#define HCI_INFO_PARAM 0x04     /* Informational Parameters */
#define HCI_STATUS_PARAM 0x05   /* Status Parameters */
#define HCI_TESTING 0x06        /* Testing Commands */

/* Opcode Command Field (OCF) values */

/* Link control commands */
#define HCI_INQUIRY 0x01
#define HCI_PERIODIC_INQUIRY 0x03
#define HCI_CREATE_CONNECTION 0x05
#define HCI_REJECT_CONNECTION_REQUEST 0x0A
#define HCI_DISCONNECT 0x06
#define HCI_PIN_CODE_REQ_REP 0x0D
#define HCI_PIN_CODE_REQ_NEG_REP 0x0E
#define HCI_LINK_KEY_REQ_REP 0x0B
#define HCI_LINK_KEY_REQ_REP_NEG 0x0C
#define HCI_SET_CONN_ENCRYPT 0x13

/* Link Policy commands */
#define HCI_HOLD_MODE 0x01
#define HCI_SNIFF_MODE 0x03
#define HCI_EXIT_SNIFF_MODE 0x04
#define HCI_PARK_MODE 0x05
#define HCI_EXIT_PARK_MODE 0x06
#define HCI_W_LINK_POLICY 0x0D

/* Host-Controller and Baseband Commands */
#define HCI_SET_EVENT_MASK 0x01
#define HCI_RESET 0x03
#define HCI_SET_EVENT_FILTER 0x05
#define HCI_WRITE_STORED_LINK_KEY 0x11
#define HCI_ROLE_CHANGE 0x12
#define HCI_WRITE_LOCAL_NAME 0x13

#define HCI_WRITE_PAGE_TIMEOUT 0x18
#define HCI_WRITE_SCAN_ENABLE 0x1A
#define HCI_WRITE_COD 0x24
#define HCI_W_FLUSHTO 0x28
#define HCI_SET_HC_TO_H_FC 0x31
#define HCI_READ_CUR_IACLAP 0x39
#define HCI_WRITE_PIN_TYPE 0x0A
#define HCI_R_STORED_LINK_KEY 0x0D
#define HCI_HOST_BUF_SIZE 0x33
#define HCI_WRITE_INQUIRY_SCAN_TYPE 0x43
#define HCI_WRITE_INQUIRY_MODE 0x45
#define HCI_WRITE_PAGE_SCAN_TYPE 0x47

/* Informational Parameters */
#define HCI_READ_LOCAL_VERSION 0x01
#define HCI_READ_LOCAL_FEATURES 0x03
#define HCI_READ_BUFFER_SIZE 0x05
#define HCI_READ_BD_ADDR 0x09

/* Status Parameters */
#define HCI_READ_FAILED_CONTACT_COUNTER 0x01
#define HCI_RESET_FAILED_CONTACT_COUNTER 0x02
#define HCI_GET_LINK_QUALITY 0x03
#define HCI_READ_RSSI 0x05

/* Testing commands */

/* Possible event codes */
#define HCI_INQUIRY_COMPLETE 0x01
#define HCI_INQUIRY_RESULT 0x02
#define HCI_CONNECTION_COMPLETE 0x03
#define HCI_CONNECTION_REQUEST 0x04
#define HCI_DISCONNECTION_COMPLETE 0x05
#define HCI_ENCRYPTION_CHANGE 0x08
#define HCI_QOS_SETUP_COMPLETE 0x0D
#define HCI_COMMAND_COMPLETE 0x0E
#define HCI_COMMAND_STATUS 0x0F
#define HCI_HARDWARE_ERROR 0x10
#define HCI_ROLE_CHANGE 0x12
#define HCI_NBR_OF_COMPLETED_PACKETS 0x13
#define HCI_MODE_CHANGE 0x14
#define HCI_RETURN_LINK_KEYS 0x15
#define HCI_PIN_CODE_REQUEST 0x16
#define HCI_LINK_KEY_REQUEST 0x17
#define HCI_LINK_KEY_NOTIFICATION 0x18
#define HCI_DATA_BUFFER_OVERFLOW 0x1A
#define HCI_MAX_SLOTS_CHANGE 0x1B

/* Success code */
#define HCI_SUCCESS 0x00
/* Possible error codes */
#define HCI_UNKNOWN_HCI_COMMAND 0x01
#define HCI_NO_CONNECTION 0x02
#define HCI_HW_FAILURE 0x03
#define HCI_PAGE_TIMEOUT 0x04
#define HCI_AUTHENTICATION_FAILURE 0x05
#define HCI_KEY_MISSING 0x06
#define HCI_MEMORY_FULL 0x07
#define HCI_CONN_TIMEOUT 0x08
#define HCI_MAX_NUMBER_OF_CONNECTIONS 0x09
#define HCI_MAX_NUMBER_OF_SCO_CONNECTIONS_TO_DEVICE 0x0A
#define HCI_ACL_CONNECTION_EXISTS 0x0B
#define HCI_COMMAND_DISSALLOWED 0x0C
#define HCI_HOST_REJECTED_DUE_TO_LIMITED_RESOURCES 0x0D
#define HCI_HOST_REJECTED_DUE_TO_SECURITY_REASONS 0x0E
#define HCI_HOST_REJECTED_DUE_TO_REMOTE_DEVICE_ONLY_PERSONAL_SERVICE 0x0F
#define HCI_HOST_TIMEOUT 0x10
#define HCI_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE 0x11
#define HCI_INVALID_HCI_COMMAND_PARAMETERS 0x12
#define HCI_OTHER_END_TERMINATED_CONN_USER_ENDED 0x13
#define HCI_OTHER_END_TERMINATED_CONN_LOW_RESOURCES 0x14
#define HCI_OTHER_END_TERMINATED_CONN_ABOUT_TO_POWER_OFF 0x15
#define HCI_CONN_TERMINATED_BY_LOCAL_HOST 0x16
#define HCI_REPETED_ATTEMPTS 0x17
#define HCI_PAIRING_NOT_ALLOWED 0x18
#define HCI_UNKNOWN_LMP_PDU 0x19
#define HCI_UNSUPPORTED_REMOTE_FEATURE 0x1A
#define HCI_SCO_OFFSET_REJECTED 0x1B
#define HCI_SCO_INTERVAL_REJECTED 0x1C
#define HCI_SCO_AIR_MODE_REJECTED 0x1D
#define HCI_INVALID_LMP_PARAMETERS 0x1E
#define HCI_UNSPECIFIED_ERROR 0x1F
#define HCI_UNSUPPORTED_LMP_PARAMETER_VALUE 0x20
#define HCI_ROLE_CHANGE_NOT_ALLOWED 0x21
#define HCI_LMP_RESPONSE_TIMEOUT 0x22
#define HCI_LMP_ERROR_TRANSACTION_COLLISION 0x23
#define HCI_LMP_PDU_NOT_ALLOWED 0x24
#define HCI_ENCRYPTION_MODE_NOT_ACCEPTABLE 0x25
#define HCI_UNIT_KEY_USED 0x26
#define HCI_QOS_NOT_SUPPORTED 0x27
#define HCI_INSTANT_PASSED 0x28
#define HCI_PAIRING_UNIT_KEY_NOT_SUPPORTED 0x29

/* Specification specific parameters */
#define HCI_BD_ADDR_LEN 6
#define HCI_LMP_FEATURES_LEN 8
#define HCI_LINK_KEY_LEN 16
#define HCI_LMP_FEAT_LEN 8

/* Command OGF */
#define HCI_LINK_CTRL_OGF 0x01 /* Link ctrl cmds */
#define HCI_LINK_POLICY_OGF 0x02    /* Link Policy Commands */
#define HCI_HC_BB_OGF 0x03 /* Host controller and baseband commands */
#define HCI_INFO_PARAM_OGF 0x04 /* Informal parameters */

/* Command OCF */
#define HCI_INQUIRY_OCF 0x01
#define HCI_SNIFF_MODE_OCF 0x03
#define HCI_EXIT_SNIFF_MODE_OCF 0x04
#define HCI_PARK_MODE_OCF 0x05
#define HCI_EXIT_PARK_MODE_OCF 0x06
#define HCI_PERIODIC_INQUIRY_OCF 0x03
#define HCI_EXIT_PERIODIC_INQUIRY_OCF 0x04
#define HCI_CREATE_CONN_OCF 0x05
#define HCI_DISCONN_OCF 0x06
#define HCI_W_LOCAL_NAME_OCF 0x13
#define HCI_W_LINK_POLICY_OCF 0x0D
#define HCI_ACCEPT_CONN_REQ_OCF 0x09
#define HCI_REJECT_CONN_REQ_OCF 0x0A
#define HCI_SET_EV_MASK_OCF 0x01
#define HCI_RESET_OCF 0x03
#define HCI_SET_EV_FILTER_OCF 0x05
#define HCI_R_STORED_LINK_KEY_OCF 0x0D
#define HCI_W_PAGE_TIMEOUT_OCF 0x18
#define HCI_W_SCAN_EN_OCF 0x1A
#define HCI_R_COD_OCF 0x23
#define HCI_W_COD_OCF 0x24
#define HCI_SET_HC_TO_H_FC_OCF 0x31
#define HCI_H_BUF_SIZE_OCF 0x33
#define HCI_H_NUM_COMPL_OCF 0x35
#define HCI_R_CUR_IACLAP_OCF 0x39
#define HCI_R_LOC_VERSION_OCF 0x01
#define HCI_R_LOC_FEATURES_OCF 0x03
#define HCI_R_BUF_SIZE_OCF 0x05
#define HCI_R_BD_ADDR_OCF 0x09
#define HCI_R_REMOTE_NAME_OCF 0x19
#define HCI_W_PIN_TYPE_OCF 0x0A
#define HCI_W_INQUIRY_SCAN_TYPE_OCF 0x43
#define HCI_W_INQUIRY_MODE_OCF 0x45
#define HCI_W_PAGE_SCAN_TYPE_OCF 0x47
#define HCI_W_PAGE_SCAN_TYPE_OCF 0x47

/* Command packet length (including ACL header)*/
#define HCI_INQUIRY_PLEN 9
#define HCI_PERIODIC_INQUIRY_PLEN 13
#define HCI_EXIT_PERIODIC_INQUIRY_PLEN 4
#define HCI_CREATE_CONN_PLEN 17
#define HCI_DISCONN_PLEN 7
#define HCI_REJECT_CONN_REQ_PLEN 11
#define HCI_ACCEPT_CONN_REQ_PLEN 11
#define HCI_PIN_CODE_REQ_REP_PLEN 27
#define HCI_PIN_CODE_REQ_NEG_REP_PLEN 10
#define HCI_LINK_KEY_REQ_REP_PLEN 26
#define HCI_LINK_KEY_REQ_REP_NEG_PLEN 10
#define HCI_SET_CONN_ENCRYPT_PLEN 7
#define HCI_WRITE_STORED_LINK_KEY_PLEN 27
#define HCI_SET_EV_MASK_PLEN 12
#define HCI_SNIFF_PLEN 14
#define HCI_W_LINK_POLICY_PLEN 8
#define HCI_RESET_PLEN 4
#define HCI_SET_EV_FILTER_PLEN 6
#define HCI_W_PAGE_TIMEOUT_PLEN 6
#define HCI_W_SCAN_EN_PLEN 5
#define HCI_R_COD_PLEN 4
#define HCI_W_COD_PLEN 7
#define HCI_W_FLUSHTO_PLEN 8
#define HCI_W_LOCAL_NAME_PLEN 252
#define HCI_SET_HC_TO_H_FC_PLEN 5
#define HCI_H_BUF_SIZE_PLEN 11
#define HCI_H_NUM_COMPL_PLEN 9
#define HCI_R_LOC_FEAT_SIZE_PLEN 4
#define HCI_R_LOC_VERS_SIZE_PLEN 4
#define HCI_R_BUF_SIZE_PLEN 4
#define HCI_R_BD_ADDR_PLEN 4
#define HCI_R_CUR_IACLAP_PLEN 4
#define HCI_R_STORED_LINK_KEY_PLEN 11
#define HCI_R_REMOTE_NAME_PLEN 14
#define HCI_W_PIN_TYPE_PLEN 5
#define HCI_W_INQUIRY_MODE_PLEN 5
#define HCI_W_INQUIRY_SCAN_TYPE_PLEN 5
#define HCI_W_PAGE_SCAN_TYPE_PLEN 5
#define HCI_W_VENDOR_CMD_PLEN 4

struct hci_evt_hdr
{
	u8_t code;
	u8_t len;
} ATTRIBUTE_PACKED;

struct hci_acl_hdr
{
	u16_t connhdl_pb_bc;
	u16_t len;
} ATTRIBUTE_PACKED;

struct hci_inq_res
{
	struct hci_inq_res *next;
	struct bd_addr bdaddr;
	u8_t cod[3];
	u8_t psrm;
	u8_t psm;
	u16_t co;
};

struct hci_link_key
{
	struct bd_addr bdaddr;
	u8_t key[16];

	struct hci_link_key *next;
};

struct hci_link
{
	struct hci_link *next;
	struct bd_addr bdaddr;
	u16_t connhdl;
	struct pbuf *p;
	u16_t len;
	u8_t pb;
	u32_t link_mode;
};

struct hci_version_info
{
	u8_t hci_version;
	u16_t hci_revision;
	u8_t lmp_version;
	u16_t manufacturer;
	u16_t lmp_subversion;
};

struct hci_pcb
{
	void *cbarg;
	s8_t num_cmd;

	u16_t acl_mtu;
	u8_t sco_mtu;
	u16_t acl_max_pkt;
	u16_t sco_max_pkt;

	u32_t flags;

	u8_t flow;
	u16_t host_num_acl;

	u16_t pkt_type;
	u8_t features[8];

	struct bd_addr bdaddr;
	struct hci_version_info info;

	struct hci_inq_res *ires;
	struct hci_link_key *keyres;

	err_t (*pin_req)(void *arg,struct bd_addr *bdaddr);
	err_t (*link_key_req)(void *arg,struct bd_addr *bdaddr);
	err_t (*cmd_complete)(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result);
	err_t (*link_key_not)(void *arg, struct bd_addr *bdaddr, u8_t *key);
	err_t (*conn_complete)(void *arg,struct bd_addr *bdaddr);
	err_t (*inq_complete)(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result);
	err_t (*wlp_complete)(void *arg, struct bd_addr *bdaddr);
	err_t (*conn_req)(void *arg,struct bd_addr *bdaddr,u8_t *cod,u8_t link_type);
};

err_t hci_init(void);
struct hci_link* hci_new(void);
void hci_reset_all(void);
void hci_event_handler(struct pbuf *p);
void hci_acldata_handler(struct pbuf *p);

err_t hci_reset();
err_t hci_read_bd_addr(void);
err_t hci_set_hc_to_h_fc(void);
err_t hci_read_buffer_size(void);
err_t hci_host_buffer_size(void);
err_t hci_read_current_lap(void);
err_t hci_write_cod(u8_t *cod);
err_t hci_close(struct hci_link *link);
err_t hci_write_inquiry_mode(u8_t mode);
err_t hci_write_page_scan_type(u8_t type);
err_t hci_write_inquiry_scan_type(u8_t type);
err_t hci_disconnect(struct bd_addr *bdaddr, u8_t reason);
err_t hci_reject_connection_request(struct bd_addr *bdaddr, u8_t reason);
err_t hci_pin_code_request_reply(struct bd_addr *bdaddr, u8_t pinlen, u8_t *pincode);
err_t hci_link_key_req_reply(struct bd_addr *bdaddr, u8_t *link_key);
err_t hci_write_stored_link_key(struct bd_addr *bdaddr, u8_t *link);
err_t hci_set_event_filter(u8_t filter_type,u8_t filter_cond_type,u8_t *cond);
err_t hci_write_page_timeout(u16_t timeout);
err_t hci_inquiry(u32_t lap,u8_t inq_len,u8_t num_resp,err_t (*inq_complete)(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result));
err_t hci_pin_code_request_neg_reply(struct bd_addr *bdaddr);
err_t hci_link_key_req_neg_reply(struct bd_addr *bdaddr);
err_t hci_write_scan_enable(u8_t scan_enable);
err_t hci_host_num_comp_packets(u16_t conhdl, u16_t num_complete);
err_t hci_sniff_mode(struct bd_addr *bdaddr, u16_t max_interval, u16_t min_interval, u16_t attempt, u16_t timeout);
err_t hci_write_link_policy_settings(struct bd_addr *bdaddr, u16_t link_policy);
err_t hci_periodic_inquiry(u32_t lap,u16_t min_period,u16_t max_period,u8_t inq_len,u8_t num_resp,err_t (*inq_complete)(void *arg,struct hci_pcb *pcb,struct hci_inq_res *ires,u16_t result));
err_t hci_exit_periodic_inquiry();
err_t hci_accecpt_conn_request(struct bd_addr *bdaddr,u8_t role);
err_t hci_set_event_mask(u64_t ev_mask);
err_t hci_read_local_version(void);
err_t hci_read_local_features(void);
err_t hci_write_local_name(u8_t *name,u8_t len);
err_t hci_write_pin_type(u8_t type);
err_t hci_read_stored_link_key();
err_t hci_read_remote_name(struct bd_addr *bdaddr);
err_t hci_vendor_specific_command(u8_t ocf,u8_t ogf,void *data,u8_t len);

err_t hci_reg_dev_info(struct bd_addr *bdaddr,u8_t *cod,u8_t psrm,u8_t psm,u16_t co);

void hci_arg(void *arg);
void hci_cmd_complete(err_t (*cmd_complete)(void *arg,struct hci_pcb *pcb,u8_t ogf,u8_t ocf,u8_t result));
void hci_connection_complete(err_t (* conn_complete)(void *arg, struct bd_addr *bdaddr));
void hci_pin_req(err_t (* pin_req)(void *arg, struct bd_addr *bdaddr));
void hci_link_key_req(err_t (* link_key_req)(void *arg, struct bd_addr *bdaddr));
void hci_link_key_not(err_t (* link_key_not)(void *arg, struct bd_addr *bdaddr, u8_t *key));
void hci_wlp_complete(err_t (* wlp_complete)(void *arg, struct bd_addr *bdaddr));
void hci_conn_req(err_t (*conn_req)(void *arg,struct bd_addr *bdaddr,u8_t *cod,u8_t link_type));

u16_t lp_pdu_maxsize();
u8_t lp_is_connected(struct bd_addr *bdaddr);
err_t lp_acl_write(struct bd_addr *bdaddr,struct pbuf *p,u16_t len,u8_t pb);
err_t lp_connect_req(struct bd_addr *bdaddr, u8_t allow_role_switch);
err_t lp_write_flush_timeout(struct bd_addr *bdaddr, u16_t flushto);

#define HCI_EVENT_PIN_REQ(pcb,bdaddr,ret) \
                         if((pcb)->pin_req != NULL) { \
                           (ret = (pcb)->pin_req((pcb)->cbarg,(bdaddr))); \
                         } else { \
                           ret = hci_pin_code_request_neg_reply(bdaddr); \
						}
#define HCI_EVENT_LINK_KEY_REQ(pcb,bdaddr,ret) \
                         if((pcb)->link_key_req != NULL) { \
                           (ret = (pcb)->link_key_req((pcb)->cbarg,(bdaddr))); \
                         } else { \
                           ret = hci_link_key_req_neg_reply(bdaddr); \
						}
#define HCI_EVENT_CONN_REQ(pcb,bdaddr,cod,linktype,ret) \
						 if((pcb)->conn_req!=NULL) \
						 (ret = (pcb)->conn_req((pcb)->cbarg,(bdaddr),(cod),(linktype)))
#define HCI_EVENT_LINK_KEY_NOT(pcb,bdaddr,key,ret) \
                              if((pcb)->link_key_not != NULL) { \
                                (ret = (pcb)->link_key_not((pcb)->cbarg,(bdaddr),(key))); \
                              }
#define HCI_EVENT_INQ_COMPLETE(pcb,result,ret) \
                              if((pcb)->inq_complete != NULL) \
                              (ret = (pcb)->inq_complete((pcb)->cbarg,(pcb),(pcb)->ires,(result)))
#define HCI_EVENT_WLP_COMPLETE(pcb,bdaddr,ret) \
                               if((pcb)->wlp_complete != NULL) \
                               (ret = (pcb)->wlp_complete((pcb)->cbarg,(bdaddr)));
#define HCI_EVENT_CONN_COMPLETE(pcb,bdaddr,ret) \
                               if((pcb)->conn_complete != NULL) \
                               (ret = (pcb)->conn_complete((pcb)->cbarg,(bdaddr)));
#define HCI_EVENT_CMD_COMPLETE(pcb,ogf,ocf,result,ret) \
                              if((pcb)->cmd_complete != NULL) \
                              (ret = (pcb)->cmd_complete((pcb)->cbarg,(pcb),(ogf),(ocf),(result)))

/* The HCI LINK lists. */
extern struct hci_link *hci_active_links; /* List of all active HCI LINKs */
extern struct hci_link *hci_tmp_link; /* Only used for temporary storage. */
extern struct hci_link_key *hci_tmp_key;

#define HCI_REG(links, nlink) do { \
							u32 level; \
							_CPU_ISR_Disable(level); \
                            nlink->next = *links; \
                            *links = nlink; \
							_CPU_ISR_Restore(level); \
                            } while(0)
#define HCI_RMV(links, nlink) do { \
							u32 level; \
							_CPU_ISR_Disable(level);\
                            if(*links == nlink) { \
                               *links = (*links)->next; \
                            } else for(hci_tmp_link = *links; hci_tmp_link != NULL; hci_tmp_link = hci_tmp_link->next) { \
                               if(hci_tmp_link->next != NULL && hci_tmp_link->next == nlink) { \
                                  hci_tmp_link->next = nlink->next; \
                                  break; \
                               } \
                            } \
                            nlink->next = NULL; \
							_CPU_ISR_Restore(level); \
                            } while(0)

#endif
