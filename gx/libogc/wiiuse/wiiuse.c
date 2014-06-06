/**
 *	@file
 *	@brief General wiimote operations.
 *
 *	The file includes functions that handle general
 *	tasks.  Most of these are functions that are part
 *	of the API.
 */

#include "wiiuse_internal.h"
#include "io.h"                         /* for wiiuse_handshake, etc */
#include "os.h"							    /* for wiiuse_os_* */

#ifdef WIN32
	#include <Winsock2.h>
#endif

#include <stdio.h>                      /* for printf, FILE */
#include <stdlib.h>                     /* for malloc, free */
#include <string.h>                     /* for memcpy, memset */

static int g_banner = 0;
static const char g_wiiuse_version_string[] = WIIUSE_VERSION;
#ifndef NEW_WIIUSE
static struct wiimote_t** __wm = NULL;
#endif

/**
 *	@brief Returns the version of the library.
 */
const char* wiiuse_version() {
	return g_wiiuse_version_string;
}

/**
 *	@brief Output FILE stream for each wiiuse_loglevel.
 */
FILE* logtarget[4];


#ifdef NEW_WIIUSE
/**
 *	@brief Specify an alternate FILE stream for a log level.
 *
 *	@param loglevel The loglevel, for which the output should be set.
 *
 *	@param logfile A valid, writeable <code>FILE*</code>, or 0, if output should be disabled.
 *
 *  The default <code>FILE*</code> for all loglevels is <code>stderr</code>
 */
void wiiuse_set_output(enum wiiuse_loglevel loglevel, FILE *logfile) {
	logtarget[(int)loglevel] = logfile;
}
/**
 *	@brief Clean up wiimote_t array created by wiiuse_init()
 */
void wiiuse_cleanup(struct wiimote_t** wm, int wiimotes) {
	int i = 0;

	if (!wm) {
		return;
	}

	WIIUSE_INFO("wiiuse clean up...");

	for (; i < wiimotes; ++i) {
		wiiuse_disconnect(wm[i]);
		wiiuse_cleanup_platform_fields(wm[i]);
		free(wm[i]);
	}

	free(wm);

	return;
}
#endif

void wiiuse_send_next_command(struct wiimote_t *wm)
{
	struct cmd_blk_t *cmd = wm->cmd_head;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) {
      return;
   }

	if(!cmd) return;
	if (cmd->state != CMD_READY) {
      return;
   }

	wiiuse_io_write(wm,cmd->data,cmd->len);

	cmd->state = CMD_SENT;
   return;
}

static __inline__ void __wiiuse_push_command(struct wiimote_t *wm,struct cmd_blk_t *cmd)
{
	uint level;

	if(!wm || !cmd) return;

	cmd->next = NULL;
	cmd->state = CMD_READY;

	_CPU_ISR_Disable(level);
	if(wm->cmd_head==NULL) {
		wm->cmd_head = wm->cmd_tail = cmd;
		wiiuse_send_next_command(wm);
	} else {
		wm->cmd_tail->next = cmd;
		wm->cmd_tail = cmd;
	}
	_CPU_ISR_Restore(level);
}

extern void __wiiuse_sensorbar_enable(int enable);

/**
 *	@brief Initialize an array of wiimote structures.
 *
 *	@param wiimotes		Number of wiimote_t structures to create.
 *
 *	@return An array of initialized wiimote_t structures.
 *
 *	@see wiiuse_connect()
 *
 *	The array returned by this function can be passed to various
 *	functions, including wiiuse_connect().
 */
#ifdef NEW_WIIUSE
struct wiimote_t** wiiuse_init(int wiimotes) {
#else
struct wiimote_t** wiiuse_init(int wiimotes, wii_event_cb event_cb)
#endif
{
	int i = 0;
#ifdef NEW_WIIUSE
	struct wiimote_t** wm = NULL;
#endif

	/*
	 *	Please do not remove this banner.
	 *	GPL asks that you please leave output credits intact.
	 *	Thank you.
	 *
	 *	This banner is only displayed once so that if you need
	 *	to call this function again it won't be intrusive.
	 */
	if (!g_banner) {
		printf("wiiuse v" WIIUSE_VERSION " loaded.\n"
		       "  Fork at http://github.com/rpavlik/wiiuse\n"
		       "  Original By: Michael Laforest <thepara[at]gmail{dot}com> http://wiiuse.net\n");
		g_banner = 1;
	}

	logtarget[0] = stderr;
	logtarget[1] = stderr;
	logtarget[2] = stderr;
	logtarget[3] = stderr;

	if (!wiimotes) {
		return NULL;
   }

#ifdef NEW_WIIUSE
   wm = malloc(sizeof(struct wiimote_t*) * wiimotes);
#else
	if (!__wm) {
		__wm = __lwp_wkspace_allocate(sizeof(struct wiimote_t*) * wiimotes);
		if(!__wm) return NULL;
		memset(__wm, 0, sizeof(struct wiimote_t*) * wiimotes);
	}
#endif

	for (i = 0; i < wiimotes; ++i) {
#ifdef NEW_WIIUSE
		wm[i] = malloc(sizeof(struct wiimote_t));
#else
		if(!__wm[i])
			__wm[i] = __lwp_wkspace_allocate(sizeof(struct wiimote_t));
#endif
		memset(__wm[i], 0, sizeof(struct wiimote_t));

		__wm[i]->unid = i;
#ifdef NEW_WIIUSE
		wiiuse_init_platform_fields(wm[i]);
#else
		#if defined(WIN32)
			__wm[i]->dev_handle = 0;
			__wm[i]->stack = WIIUSE_STACK_UNKNOWN;
			__wm[i]->normal_timeout = WIIMOTE_DEFAULT_TIMEOUT;
			__wm[i]->exp_timeout = WIIMOTE_EXP_TIMEOUT;
			__wm[i]->timeout = __wm[i]->normal_timeout;
		#elif defined(GEKKO)
			__wm[i]->sock = NULL;
			__wm[i]->bdaddr = *BD_ADDR_ANY;
			__wm[i]->event_cb = event_cb;
			wiiuse_init_cmd_queue(__wm[i]);
		#elif defined(unix)
			__wm[i]->bdaddr = *BDADDR_ANY;
			__wm[i]->out_sock = -1;
			__wm[i]->in_sock = -1;
		#endif
#endif

		__wm[i]->state = WIIMOTE_INIT_STATES;
		__wm[i]->flags = WIIUSE_INIT_FLAGS;

		__wm[i]->event = WIIUSE_NONE;

		__wm[i]->exp.type = EXP_NONE;
#ifdef NEW_WIIUSE
		wm[i]->expansion_state = 0;
#endif

		wiiuse_set_aspect_ratio(__wm[i], WIIUSE_ASPECT_4_3);
		wiiuse_set_ir_position(__wm[i], WIIUSE_IR_ABOVE);

#ifdef NEW_WIIUSE
		wm[i]->orient_threshold = 0.5f;
		wm[i]->accel_threshold = 5;
#endif

		__wm[i]->accel_calib.st_alpha = WIIUSE_DEFAULT_SMOOTH_ALPHA;
	}

	return __wm;
}

/**
 *	@brief	The wiimote disconnected.
 *
 *	@param wm	Pointer to a wiimote_t structure.
 */
void wiiuse_disconnected(struct wiimote_t* wm) {
	if (!wm)	{
		return;
	}

	WIIUSE_INFO("Wiimote disconnected [id %i].", wm->unid);

	/* disable the connected flag */
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);

	/* reset a bunch of stuff */
	wm->leds = 0;
	wm->state = WIIMOTE_INIT_STATES;
#ifdef NEW_WIIUSE
	wm->read_req = NULL;
#endif
#ifndef WIIUSE_SYNC_HANDSHAKE
	wm->handshake_state = 0;
#endif
	wm->btns = 0;
	wm->btns_held = 0;
	wm->btns_released = 0;

	wm->event = WIIUSE_DISCONNECT;
}

/**
 *	@brief Set flags for the specified wiimote.
 *
 *	@param wm			Pointer to a wiimote_t structure.
 *	@param enable		Flags to enable.
 *	@param disable		Flags to disable.
 *
 *	@return The flags set after 'enable' and 'disable' have been applied.
 *
 *	The values 'enable' and 'disable' may be any flags OR'ed together.
 *	Flags are defined in wiiuse.h.
 */
int wiiuse_set_flags(struct wiimote_t* wm, int enable, int disable) {
	if (!wm) {
      return 0;
   }

	/* remove mutually exclusive flags */
	enable &= ~disable;
	disable &= ~enable;

	wm->flags |= enable;
	wm->flags &= ~disable;

	return wm->flags;
}

/**
 *	@brief	Set if the wiimote should report motion sensing.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param status	1 to enable, 0 to disable.
 *
 *	Since reporting motion sensing sends a lot of data,
 *	the wiimote saves power by not transmitting it
 *	by default.
 */
void wiiuse_motion_sensing(struct wiimote_t* wm, int status) 
{
	if (status) {
		WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_ACC);
	} else {
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_ACC);
	}

	wiiuse_status(wm,NULL);
}

/**
 *	@brief	Toggle the state of the rumble.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 */
void wiiuse_toggle_rumble(struct wiimote_t* wm) 
{
	if (!wm)	{
		return;
	}

#ifdef NEW_WIIUSE
	WIIMOTE_TOGGLE_STATE(wm, WIIMOTE_STATE_RUMBLE);
	if(!WIIMOTE_IS_SET(wm,WIIMOTE_STATE_HANDSHAKE_COMPLETE)) return;

	wiiuse_set_leds(wm,wm->leds,NULL);
#else
	wiiuse_rumble(wm, !WIIMOTE_IS_SET(wm, WIIMOTE_STATE_RUMBLE));
#endif
}

/**
 *	@brief	Enable or disable the rumble.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param status	1 to enable, 0 to disable.
 */
void wiiuse_rumble(struct wiimote_t* wm, int status) 
{
	if (!wm || !WIIMOTE_IS_CONNECTED(wm)) {
		return;
	}

#ifdef NEW_WIIUSE
	/* make sure to keep the current lit leds */
	buf = wm->leds;

	if (status) {
		WIIUSE_DEBUG("Starting rumble...");
		WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_RUMBLE);
		buf |= 0x01;
	} else {
		WIIUSE_DEBUG("Stopping rumble...");
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_RUMBLE);
		buf &= ~(0x01);
	}

	/* preserve IR state */
	if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR)) {
		buf |= 0x04;
	}

	wiiuse_send(wm, WM_CMD_RUMBLE, &buf, 1);
#else
	if (status && WIIMOTE_IS_SET(wm,WIIMOTE_STATE_RUMBLE)) return;
	else if(!status && !WIIMOTE_IS_SET(wm,WIIMOTE_STATE_RUMBLE)) return;
	wiiuse_toggle_rumble(wm);
#endif
}

/**
 *	@brief	Set the enabled LEDs.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param leds		What LEDs to enable.
 *
 *	\a leds is a bitwise or of WIIMOTE_LED_1, WIIMOTE_LED_2, WIIMOTE_LED_3, or WIIMOTE_LED_4.
 */
void wiiuse_set_leds(struct wiimote_t *wm,int leds,cmd_blk_cb cb)
{
	ubyte buf;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) {
      return;
   }

	/* remove the lower 4 bits because they control rumble */
	wm->leds = (leds & 0xf0);

	/* make sure if the rumble is on that we keep it on */
	if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_RUMBLE)) {
		wm->leds |= 0x01;
   }

	buf = wm->leds;

#ifdef NEW_WIIUSE
	wiiuse_send(wm, WM_CMD_LED, &buf, 1);
#else
	wiiuse_sendcmd(wm, WM_CMD_LED, &buf, 1, cb);
#endif
}

/**
 *	@brief	Set the report type based on the current wiimote state.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *
 *	@return The report type sent.
 *
 *	The wiimote reports formatted packets depending on the
 *	report type that was last requested.  This function will
 *	update the type of report that should be sent based on
 *	the current state of the device.
 */
#ifdef NEW_WIIUSE
int wiiuse_set_report_type(struct wiimote_t* wm)
#else
int wiiuse_set_report_type(struct wiimote_t *wm,cmd_blk_cb cb)
#endif
{
	ubyte buf[2];
	int motion,ir,exp;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) {
      return 0;
   }

	buf[0] = (WIIMOTE_IS_FLAG_SET(wm, WIIUSE_CONTINUOUS) ? 0x04 : 0x00);	/* set to 0x04 for continuous reporting */
	buf[1] = 0x00;

	/* if rumble is enabled, make sure we keep it */
	if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_RUMBLE)) {
		buf[0] |= 0x01;
	}

	motion = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_ACC);
	exp = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP);
	ir = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR);

	if (motion && ir && exp)   {
      buf[1] = WM_RPT_BTN_ACC_IR_EXP;
   } else if (motion && exp) {
      buf[1] = WM_RPT_BTN_ACC_EXP;
   } else if (motion && ir) {
      buf[1] = WM_RPT_BTN_ACC_IR;
   } else if (ir && exp) {
      buf[1] = WM_RPT_BTN_IR_EXP;
   } else if (ir) {
      buf[1] = WM_RPT_BTN_ACC_IR;
   } else if (exp) {
      buf[1] = WM_RPT_BTN_EXP;
   } else if (motion) {
      buf[1] = WM_RPT_BTN_ACC;
   } else {
      buf[1] = WM_RPT_BTN;
   }

	WIIUSE_DEBUG("Setting report type: 0x%x", buf[1]);

#ifdef NEW_WIIUSE
	exp = wiiuse_send(wm, WM_CMD_REPORT_TYPE, buf, 2);
	if (exp <= 0) {
		return exp;
	}
#else
	wiiuse_sendcmd(wm,WM_CMD_REPORT_TYPE,buf,2,cb);
#endif

	return buf[1];
}

/**
 *	@brief Request the wiimote controller status.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *
 *	Controller status includes: battery level, LED status, expansions
 */
void wiiuse_status(struct wiimote_t *wm,cmd_blk_cb cb)
{
	ubyte buf = 0;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) {
      return;
   }
	
	WIIUSE_DEBUG("Requested wiimote status.");

#ifdef NEW_WIIUSE
	wiiuse_send(wm, WM_CMD_CTRL_STATUS, &buf, 1);
#else
	wiiuse_sendcmd(wm,WM_CMD_CTRL_STATUS,&buf,1,cb);
#endif
}

/**
 *	@brief	Read data from the wiimote (event version).
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param buffer	An allocated buffer to store the data as it arrives from the wiimote.
 *					Must be persistent in memory and large enough to hold the data.
 *	@param addr		The address of wiimote memory to read from.
 *	@param len		The length of the block to be read.
 *
 *	The library can only handle one data read request at a time
 *	because it must keep track of the buffer and other
 *	events that are specific to that request.  So if a request
 *	has already been made, subsequent requests will be added
 *	to a pending list and be sent out when the previous
 *	finishes.
 */
#ifdef NEW_WIIUSE
int wiiuse_read_data(struct wiimote_t* wm, byte* buffer, unsigned int addr, uint16_t len) {
	return wiiuse_read_data_cb(wm, NULL, buffer, addr, len);
#else
int wiiuse_read_data(struct wiimote_t *wm,ubyte *buffer,uint addr,uword len,cmd_blk_cb read_cb) {
	struct op_t *op;
	struct cmd_blk_t *cmd;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return 0;
	if(!buffer || !len) return 0;
	
	cmd = (struct cmd_blk_t*)__lwp_queue_get(&wm->cmdq);
	if(!cmd) return 0;

	cmd->cb = read_cb;
	cmd->len = 7;

	op = (struct op_t*)cmd->data;
	op->cmd = WM_CMD_READ_DATA;
	op->buffer = buffer;
	op->wait = len;
	op->readdata.addr = BIG_ENDIAN_LONG(addr);
	op->readdata.size = BIG_ENDIAN_SHORT(len);
	__wiiuse_push_command(wm,cmd);

	return 1;
#endif
}

/**
 *	@brief	Write data to the wiimote.
 *
 *	@param wm			Pointer to a wiimote_t structure.
 *	@param addr			The address to write to.
 *	@param data			The data to be written to the memory location.
 *	@param len			The length of the block to be written.
 */
#ifdef NEW_WIIUSE
int wiiuse_write_data(struct wiimote_t* wm, unsigned int addr, const byte* data, byte len) {
	byte buf[21] = {0};		/* the payload is always 23 */

	byte * bufPtr = buf;
	if (!wm || !WIIMOTE_IS_CONNECTED(wm)) {
		return 0;
	}
	if (!data || !len) {
		return 0;
	}

	WIIUSE_DEBUG("Writing %i bytes to memory location 0x%x...", len, addr);

#ifdef WITH_WIIUSE_DEBUG
	{
		int i = 0;
		printf("Write data is: ");
		for (; i < len; ++i) {
			printf("%x ", data[i]);
		}
		printf("\n");
	}
#endif

	/* the offset is in big endian */
	buffer_big_endian_uint32_t(&bufPtr, (uint32_t)addr);

	/* length */
	buffer_big_endian_uint8_t(&bufPtr, len);

	/* data */
	memcpy(bufPtr, data, len);

	wiiuse_send(wm, WM_CMD_WRITE_DATA, buf, 21);
	return 1;
}
#else
int wiiuse_write_data(struct wiimote_t *wm,uint addr,ubyte *data,ubyte len,cmd_blk_cb cb) {
	struct op_t *op;
	struct cmd_blk_t *cmd;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) {
      return 0;
   }
	if(!data || !len) {
      return 0;
   }

	WIIUSE_DEBUG("Writing %i bytes to memory location 0x%x...", len, addr);

#ifdef WITH_WIIUSE_DEBUG
	{
		int i = 0;
		printf("Write data is: ");
		for (; i < len; ++i) {
			printf("%x ", data[i]);
		}
		printf("\n");
	}
#endif

	cmd = (struct cmd_blk_t*)__lwp_queue_get(&wm->cmdq);
	if(!cmd) return 0;

	cmd->cb = cb;
	cmd->len = 22;

	op = (struct op_t*)cmd->data;
	op->cmd = WM_CMD_WRITE_DATA;
	op->buffer = NULL;
	op->wait = 0;
	op->writedata.addr = BIG_ENDIAN_LONG(addr);
	op->writedata.size = (len&0x0f);
	memcpy(op->writedata.data,data,len);
	memset(op->writedata.data+len,0,(16 - len));
	__wiiuse_push_command(wm,cmd);

	return 1;
}
#endif

int wiiuse_write_streamdata(struct wiimote_t *wm,ubyte *data,ubyte len,cmd_blk_cb cb)
{
	struct cmd_blk_t *cmd;

	if(!wm || !WIIMOTE_IS_CONNECTED(wm)) return 0;
	if(!data || !len || len>20) return 0;

	cmd = (struct cmd_blk_t*)__lwp_queue_get(&wm->cmdq);
	if(!cmd) return 0;

	cmd->cb = cb;
	cmd->len = 22;
	cmd->data[0] = WM_CMD_STREAM_DATA;
	cmd->data[1] = (len<<3);
	memcpy(cmd->data+2,data,len);
	__wiiuse_push_command(wm,cmd);

	return 1;
}

int wiiuse_sendcmd(struct wiimote_t *wm,ubyte report_type,ubyte *msg,int len,cmd_blk_cb cb)
{
	struct cmd_blk_t *cmd;

	cmd = (struct cmd_blk_t*)__lwp_queue_get(&wm->cmdq);
	if(!cmd) return 0;

	cmd->cb = cb;
	cmd->len = (1+len);

	cmd->data[0] = report_type;
	memcpy(cmd->data+1,msg,len);
	if(report_type!=WM_CMD_READ_DATA && report_type!=WM_CMD_CTRL_STATUS)
		cmd->data[1] |= 0x02;

	WIIUSE_DEBUG("Pushing command: %02x %02x", cmd->data[0], cmd->data[1]);
	__wiiuse_push_command(wm,cmd);

	return 1;
}
