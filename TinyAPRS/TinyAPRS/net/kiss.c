#include "kiss.h"

#include <cfg/compiler.h>
#include <algo/rand.h>

#define LOG_LEVEL  KISS_LOG_LEVEL
#define LOG_FORMAT KISS_LOG_FORMAT
#include <cfg/log.h>

#include "global.h"

#define KISS_QUEUE CONFIG_KISS_QUEUE

KFile *kiss_fd;

static kiss_exit_callback_t exitCallback = 0;

#if KISS_QUEUE > 0
ticks_t kiss_queue_ts;
uint8_t kiss_queue_state;
size_t kiss_queue_len = 0;
struct Kiss_msg kiss_queue[KISS_QUEUE];
#endif

uint8_t kiss_txdelay;
uint8_t kiss_txtail;
uint8_t kiss_persistence;
uint8_t kiss_slot_time;
uint8_t kiss_duplex;

static void kiss_cmd_process(struct Kiss_msg *k);

static struct Kiss_msg k = {.pos = 0};

void kiss_init(KFile *fd, uint8_t *buf, uint16_t bufLen, kiss_exit_callback_t hook)
{
	kiss_txdelay = 50;
	kiss_persistence = 63;
	kiss_txtail = 5;
	kiss_slot_time = 10;
	kiss_duplex = KISS_DUPLEX_HALF;

	kiss_fd = fd;

	exitCallback = hook;

	//NOTE - Atmega328P has limited 2048 RAM, so here we have to use shared read buffer to save memory
	// NO queue for kiss message
	k.buf = buf;			// Shared buffer
	k.bufLen = bufLen; // buffer length, should be >= CONFIG_AX25_FRAME_BUF_LEN
}

INLINE void kiss_parse(int c){
	static bool escaped = false;
	// sanity checks
	// no serial input in last 2 secs?
	if ((k.pos != 0) && (timer_clock() - k.last_tick  >  ms_to_ticks(2000L))) {
		LOG_INFO("Serial - Timeout\n");
		k.pos = 0;
	}

	// about to overflow buffer? reset
	if (k.pos >= (k.bufLen - 2)) {
		LOG_INFO("Serial - Packet too long %d >= %d\n", k.pos, k.bufLen - 2);
		k.pos = 0;
	}

	if (c == KISS_FEND) {
		if ((!escaped) && (k.pos > 0)) {
			kiss_cmd_process(&k);
		}

		k.pos = 0;
		escaped = false;
		return;
	} else if (c == KISS_FESC) {
		escaped = true;
		return;
	} else if (c == KISS_TFESC) {
		if (escaped) {
			escaped = false;
			c = KISS_FESC;
		}
	} else if (c == KISS_TFEND) {
		if (escaped) {
			escaped = false;
			c = KISS_FEND;
		}
	} else if (escaped) {
		escaped = false;
	}

	k.buf[k.pos] = c & 0xff;
	k.pos++;
	k.last_tick = timer_clock();
}

void kiss_serial_poll()
{
	int c;
	c = kfile_getc(kiss_fd); // Make sure CONFIG_SERIAL_RXTIMEOUT = 0
	if (c == EOF) {
		return;
	}
	kiss_parse(c);
}

static void kiss_cmd_process(struct Kiss_msg *k)
{
	uint8_t cmd;
	uint8_t port;

	// Check return command
	if(k->pos == 1 && k->buf[0] == KISS_CMD_Return){
		LOG_INFO("Kiss - exiting");
		k->pos=0;
		if(exitCallback){
			exitCallback();
		}
		return;
	}

	// the first byte of KISS message is for command and port
	cmd = k->buf[0] & 0x0f;
	port = k->buf[0] >> 4;

	if (port > 0) {
		// not supported yet.
		return;
	}

	if (k->pos < 2) {
		LOG_INFO("Kiss - discarding packet - too short\n");
		return;
	}

	switch(cmd){
		case KISS_CMD_DATA:{
			LOG_INFO("Kiss - queuing message\n");
			kiss_queue_message(/*port = 0*/ k->buf + 1, k->pos - 1);
			break;
		}

		case KISS_CMD_TXDELAY:{
			LOG_INFO("Kiss - setting txdelay %d\n", k->buf[1]);
			if(k->buf[1] > 0){
				kiss_txdelay = k->buf[1];
			}
			break;
		}


		case KISS_CMD_P:{
			LOG_INFO("Kiss - setting persistence %d\n", k->buf[1]);
			if(k->buf[1] > 0){
				kiss_persistence = k->buf[1];
			}
			break;
		}

		case KISS_CMD_SlotTime:{
			LOG_INFO("Kiss - setting slot_time %d\n", k->buf[1]);
			if(k->buf[1] > 0){
				kiss_slot_time = k->buf[1];
			}
			break;
		}

		case KISS_CMD_TXtail:{
			LOG_INFO("Kiss - setting txtail %d\n", k->buf[1]);
			if(k->buf[1] > 0){
				kiss_txtail = k->buf[1];
			}
			break;
		}

		case KISS_CMD_FullDuplex:{
			LOG_INFO("Kiss - setting duplex %d\n", k->buf[1]);
			kiss_duplex = k->buf[1];
			break;
		}
	}// end of switch(cmd)
}

#define ACTIVE_CSMA_ENABLED 1
#if KISS_QUEUE == 0 && ACTIVE_CSMA_ENABLED == 1
static void kiss_csma(AX25Ctx *ax25Ctx, uint8_t *buf, size_t len) {
	bool sent = false;
	while (!sent) {
		if (!/*ctx->dcd*/(&g_afsk)->hdlc.rxstart) {
			uint8_t tp = rand() & 0xFF;
			if (tp < kiss_persistence) {
				ax25_sendRaw(ax25Ctx, buf, len);
				sent = true;
			} else {
				ticks_t start = timer_clock();
				while (timer_clock() - start < ms_to_ticks(kiss_slot_time * 10)) {
					cpu_relax();
				}
			}
		} else {
			while (!sent && /*kiss_ax25->dcd*/ (&g_afsk)->hdlc.rxstart) {
				// Continously poll the modem for data
				// while waiting, so we don't overrun
				// receive buffers
				ax25_poll(ax25Ctx);
				if ((&g_afsk)->status != 0) {
					// If an overflow or other error
					// occurs, we'll back off and drop
					// this packet silently.
					(&g_afsk)->status = 0;
					sent = true;
				}
			}
		}
	}
}
#endif

void kiss_queue_message(/*channel = 0*/ uint8_t *buf, size_t len){
#if KISS_QUEUE > 0
	if (kiss_queue_len == KISS_QUEUE)
		return;

	memcpy(kiss_queue[kiss_queue_len].buf, buf, len);
	kiss_queue[kiss_queue_len].pos = len;
	kiss_queue_len ++;
#else
	LOG_INFO("Kiss - queue disabled, sending message\n");
#if ACTIVE_CSMA_ENABLED
	// perform an active CSMA detection here
	kiss_csma(&g_ax25, buf, len);
#else
	ax25_sendRaw(&g_ax25, buf, len);
#endif

#endif
}

void kiss_queue_process()
{
#if KISS_QUEUE > 0
	uint8_t random;

	if (kiss_queue_len == 0) {
		return;
	}
/*
	if (kiss_afsk->cd) {
		return;
	}
*/
	if (kiss_ax25->dcd) {
		return;
	}

	if (kiss_queue_state == KISS_QUEUE_DELAYED) {
		if (timer_clock() - kiss_queue_ts <= ms_to_ticks(kiss_slot_time * 10)) {
			return;
		}
		LOG_INFO("Queue released\n");
	}

	random = (uint32_t)rand() & 0xff;
	LOG_INFO("Queue random is %d\n", random);
	if (random > kiss_persistence) {
		LOG_INFO("Queue delayed for %dms\n", kiss_slot_time * 10);

		kiss_queue_state = KISS_QUEUE_DELAYED;
		kiss_queue_ts = timer_clock();

		return;
	}

	LOG_INFO("Queue sending packets: %d\n", kiss_queue_len);
	for (size_t i = 0; i < kiss_queue_len; i++) {
		ax25_sendRaw(kiss_ax25, kiss_queue[i].buf, kiss_queue[i].pos);
	}

	kiss_queue_len = 0;
	kiss_queue_state = KISS_QUEUE_IDLE;
#endif
}


void kiss_send_host(uint8_t port, uint8_t *buf, size_t len)
{
	size_t i;

	kfile_putc(KISS_FEND, kiss_fd);
	kfile_putc((port << 4) & 0xf0, kiss_fd);

	for (i = 0; i < len; i++) {

		uint8_t c = buf[i];

		if (c == KISS_FEND) {
			kfile_putc(KISS_FESC, kiss_fd);
			kfile_putc(KISS_TFEND, kiss_fd);
			continue;
		}

		kfile_putc(c, kiss_fd);

		if (c == KISS_FESC) {
			kfile_putc(KISS_TFESC, kiss_fd);
		}
	}

	kfile_putc(KISS_FEND, kiss_fd);
}

