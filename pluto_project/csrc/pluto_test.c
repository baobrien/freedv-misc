#include <iio.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <liquid/liquid.h>
#include <complex.h>
#include <stdbool.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>

#include "freedv-tdma/tdma.h"
#include "tdma_testframer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

#define R_TO_M 0.0009765625
#define M_TO_R 3276

#define IIO_BUF_SIZE 4096

typedef struct _tdma_burst {
    float complex *     tx_buffer;
    int64_t             tx_time;
	size_t				n_tx_samps;
	struct _tdma_burst* next;
} tdma_burst;

typedef struct {
	tdma_burst *first;
	pthread_mutex_t *queue_lock;
} tx_queue;

struct tx_thread_stuff {
	struct iio_device *tx_dev;
	int rate_decim;
	int nout;
	int rate_bb;
	float f_shift;
	tx_queue tx_baseband_queue;
	volatile bool tx_quit;
};

int cb_tx_burst(tdma_t * tdma,COMP* samples, size_t n_samples,i64 timestamp,void * cb_data){
    tx_queue * tx_q = (tx_queue*) cb_data;

	tdma_burst * tx_stuff = malloc(sizeof(tdma_burst));
	tx_stuff->tx_buffer = malloc(sizeof(complex float)*n_samples);
    tx_stuff->tx_time = timestamp;
	tx_stuff->next = NULL;
	tx_stuff->n_tx_samps = n_samples;

	// cast from COMP* into float complex* because both have the same memory layout
    memcpy(tx_stuff->tx_buffer,(float complex*)samples,sizeof(float complex)*n_samples);

	pthread_mutex_lock(tx_q->queue_lock);
	tdma_burst * p = tx_q->first;
	if (p == NULL) {
		tx_q->first = tx_stuff;
	} else {
		while(p->next != NULL) {
			p = p->next;
		}
		p->next = tx_stuff;
	}
	pthread_mutex_unlock(tx_q->queue_lock);
	return 0;
}

/* Convert Comp. Short. 16 bit samps from the pluto into complex float */
static void cs16_to_cf32(complex float * restrict out, short * const in, size_t n, float mult) {
	// the C standard defines complex float as the same as an array of [r,i]
	float * restrict out_f = (float*) out;
	for (size_t i = 0; i < n * 2; i++) {
		out_f[i] = ((float)in[i]) * mult;
	}
}

static void cf32_to_cs16(short * const out, complex float * restrict in, size_t n, float mult) {
	float * const in_f = (float*) in;
	for (size_t i = 0; i < n * 2; i++) {
		out[i] = (short)(in_f[i] * mult);
	}
}

void *tx_thread_entry(void *args){
	int64_t tx_samp_count = 0;
	float f_shift;
	int rate_bb;
	int rate_decim;
	int err;
	int nout;

	struct tx_thread_stuff* tts = (struct tx_thread_stuff*) args;

	struct iio_device *tx_dev;
	struct iio_channel *tx0_i, *tx0_q;
	struct iio_buffer *txbuf;
	
	tx_dev = tts->tx_dev;
	rate_decim = tts->rate_decim;
	f_shift = tts->f_shift;
	rate_bb = tts->rate_bb;
	nout = tts->nout;

	iirinterp_crcf iir_uc = iirinterp_crcf_create_default(rate_decim, 8);
    nco_crcf upmixer = nco_crcf_create(LIQUID_NCO);

    nco_crcf_set_phase(upmixer, 0.0f);
    nco_crcf_set_frequency(upmixer,2.*M_PI*(f_shift/(float)rate_bb));

	tx0_i = iio_device_find_channel(tx_dev, "voltage0", true);
	tx0_q = iio_device_find_channel(tx_dev, "voltage1", true);

	iio_channel_enable(tx0_i);
	iio_channel_enable(tx0_q);

	txbuf = iio_device_create_buffer(tx_dev, IIO_BUF_SIZE, false);

	err = iio_buffer_set_blocking_mode(txbuf, true);

	if (err != 0) {
		printf("ERR iio_buffer_set_blocking_mode: %s\n",strerror(-err));
	}
	complex float * tx_lb_buf = (complex float*) malloc(sizeof(complex float) * nout * rate_decim);
	complex float * tx_bb_buf = (complex float*) malloc(sizeof(complex float) * nout * rate_decim);
	complex float * burst_buf_ptr;
	size_t burst_samps;
	int64_t burst_start;
	tdma_burst* current_burst = NULL;
	while (true) {
		// If no burst is pending, check queue and pop one off
		if	(current_burst == NULL) {
			pthread_mutex_lock(tts->tx_baseband_queue.queue_lock);
			if (tts->tx_baseband_queue.first != NULL) {
				tdma_burst* cur = tts->tx_baseband_queue.first;
				tts->tx_baseband_queue.first = cur->next;
				current_burst = cur;
			}
			pthread_mutex_unlock(tts->tx_baseband_queue.queue_lock);
			
			if (current_burst != NULL) {
				burst_samps = current_burst->n_tx_samps * rate_decim;
				//burst_buf_ptr = current_burst->tx_buffer;
				burst_start = (current_burst->tx_time - 55) * rate_decim;

				iirinterp_crcf_execute_block(iir_uc, current_burst->tx_buffer,  current_burst->n_tx_samps, tx_lb_buf);
				nco_crcf_mix_block_up(upmixer, tx_lb_buf, tx_bb_buf, nout * rate_decim);
				burst_buf_ptr = tx_bb_buf;

			}
		} 
		void *p_dat, *p_end;
		size_t p_inc, p_samps;
		
		p_inc = iio_buffer_step(txbuf);
		p_end = iio_buffer_end(txbuf);
		p_dat = iio_buffer_first(txbuf, tx0_i);
		p_samps = (p_end-p_dat)/p_inc;

		if (current_burst == NULL) {
			// No pending burst, send silence
			memset(p_dat, 0, p_end-p_dat);
		} else if (burst_start < tx_samp_count) {
			// Pending burst is already old. Clear and send silence.
			memset(p_dat, 0, p_end-p_dat);
			free(current_burst->tx_buffer);
			free(current_burst);
			current_burst = NULL;
			burst_start = 0;
		} else if (burst_start <= tx_samp_count + p_samps) {
			// This buffer will contain a burst
			// How many samples from the start of this buffer does the burst start?
			size_t burst_start_offset = (size_t) (burst_start - tx_samp_count);

			// Fill in first samps with zeros and bump front end pointer
			if (burst_start_offset > 0) {
				memset(p_dat, 0, p_inc*burst_start_offset);
				p_dat += (p_inc*burst_start_offset);
			}
			// How many samples left in buffer after burst starts
			size_t subburst_n_samps = p_samps - burst_start_offset;

			// Cap subburst_n_samps if the burst ends before the next buffer
			if (subburst_n_samps > burst_samps) {
				subburst_n_samps = burst_samps;
			}

			// Copy and convert TX samples
			cf32_to_cs16((short*)p_dat, burst_buf_ptr, subburst_n_samps, M_TO_R);

			// Update burst counters to reflect TX'ed samples
			burst_start += subburst_n_samps;
			burst_buf_ptr += subburst_n_samps;
			burst_samps -= subburst_n_samps;

			// If there aren't any more burst samps, end this burst
			if (burst_samps == 0) {
				free(current_burst->tx_buffer);
				free(current_burst);
				current_burst = NULL;
				burst_start = 0;

				// Fill in end of burst with silence
				size_t remaining_samps = p_samps - burst_start_offset - subburst_n_samps;
				memset(p_dat + (p_inc*subburst_n_samps), 0, remaining_samps * p_inc);
			}
		} else {
			// Pending burst has not yet started. Send silence
			memset(p_dat, 0, p_end-p_dat);
		}
		iio_buffer_push(txbuf);
		tx_samp_count += p_samps;

		if (tts->tx_quit) {
			break;
		}
	}

	iio_buffer_destroy(txbuf);
	free(tx_lb_buf);
	free(tx_bb_buf);
	if (current_burst != NULL) {
		free(current_burst->tx_buffer);
		free(current_burst);
	}
	iirinterp_crcf_destroy(iir_uc);
	nco_crcf_destroy(upmixer);
	return NULL;
}

int main (int argc, char **argv)
{
	struct iio_device *rx_dev, *tx_dev;
	struct iio_channel *rx0_i, *rx0_q;
	struct iio_buffer *rxbuf;
    
	struct iio_context *ctx_rx, *ctx_tx;
	struct iio_device *phy;

	struct tx_thread_stuff tts;

    const uint64_t rf_center = 910000000; //Center RF frequency of TDMA signal
    const uint64_t rate_bb = 288000;  //Radio Baseband Freq
    const uint64_t rate_mdm = 48000;  //Modem freq
    const int rate_decim = rate_bb/rate_mdm;
    const float f_shift = 45000;
    uint64_t rf_bbf = rf_center - (uint64_t)f_shift;

	iirdecim_crcf iir_dc = iirdecim_crcf_create_default(rate_decim, 8);
    nco_crcf downmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(downmixer, 0.0f);
    nco_crcf_set_frequency(downmixer,2.*M_PI*(f_shift/(float)rate_bb));

    struct TDMA_MODE_SETTINGS mode = FREEDV_4800T;
    tdma_t * tdma = tdma_create(mode);
    const int nout = tdma_nout(tdma);
	const int nin = tdma_nin(tdma);

    tdma->tx_multislot_delay = 9;

    tdma_test_framer * ttf = ttf_create(tdma);
	pthread_t tx_thread;
	pthread_mutex_t tx_queue_lock;

	cbuffercf in1_buffer = cbuffercf_create(nin*rate_decim+4096);
     
	//ctx = iio_create_context_from_uri("ip:192.168.2.1");
	ctx_rx = iio_create_context_from_uri("local:");
	ctx_tx = iio_context_clone(ctx_rx);

	phy = iio_context_find_device(ctx_rx, "ad9361-phy");
	rx_dev = iio_context_find_device(ctx_rx, "cf-ad9361-lpc");
	tx_dev = iio_context_find_device(ctx_tx, "cf-ad9361-dds-core-lpc");

	iio_channel_attr_write_longlong(
		iio_device_find_channel(phy, "altvoltage0", true),
		"frequency", rf_bbf);

	iio_channel_attr_write_longlong(
		iio_device_find_channel(phy, "altvoltage1", true),
		"frequency", rf_bbf);

	iio_channel_attr_write_longlong(
		iio_device_find_channel(phy, "voltage0", false),
		"sampling_frequency", rate_bb*8);

    iio_channel_attr_write_longlong(
        iio_device_find_channel(rx_dev, "voltage0", false),
        "sampling_frequency", rate_bb);

    iio_channel_attr_write_longlong(
        iio_device_find_channel(tx_dev, "voltage0", true),
        "sampling_frequency", rate_bb);
		

	pthread_mutex_init(&tx_queue_lock, NULL);
	tts.tx_dev = tx_dev;
	tts.rate_decim = rate_decim;
	tts.tx_baseband_queue.first = NULL;
	tts.tx_baseband_queue.queue_lock = &tx_queue_lock;
	tts.nout = nout;
	tts.f_shift = f_shift;
	tts.rate_bb = rate_bb;


	tx_queue * tx_bursts = &tts.tx_baseband_queue;
	// tx_bursts.first = NULL;
    
	tdma_set_tx_burst_cb(tdma,cb_tx_burst,(void*)tx_bursts);

	rx0_i = iio_device_find_channel(rx_dev, "voltage0", 0);
	rx0_q = iio_device_find_channel(rx_dev, "voltage1", 0);
 
	iio_channel_enable(rx0_i);
	iio_channel_enable(rx0_q);

	rxbuf = iio_device_create_buffer(rx_dev, IIO_BUF_SIZE, false);
	if (!rxbuf) {
		perror("Could not create RX buffer");
		exit(1);
	}

	pthread_create(&tx_thread, NULL, tx_thread_entry, (void*)&tts);

	complex float cfibuff[IIO_BUF_SIZE];
	complex float *rxdc1  = (complex float*) malloc(sizeof(complex float) * nin * rate_decim);
	complex float *rxtdma = (complex float*) malloc(sizeof(complex float) * nin);

	bool run = true;
	int loop_iter = 0;
	uint64_t rx_samp_count = 0;

    ttf->print_enable = true;
	ttf->tx_enable = true;
	ttf->tx_master = false;
	ttf->tx_id = 101;
	tdma->ignore_rx_on_tx = true;

	if (argc > 1) {
		ttf->tx_master = true;
		ttf->tx_id = 200;
	}

	bool in_tx = false;
	while (true) {
		run = true;
		while(run) {
			void *p_dat, *p_end;
			size_t p_inc, p_samps;
			// Do we have enough room in the output cbuf for a full chunk of samples?
			if(cbuffercf_max_size(in1_buffer) - cbuffercf_size(in1_buffer) < IIO_BUF_SIZE) {
				run = false;
				continue;
			}
			int ret = iio_buffer_refill(rxbuf);
			// Do we have incoming samples?
			if(ret == -EAGAIN) {
				run = false;
				continue;
			}
			if(ret < 0) {
				printf("err iio_buffer_refill: %s\n", strerror(-ret));
			}
			p_dat = iio_buffer_first(rxbuf, rx0_i);
			p_end = iio_buffer_end(rxbuf);
			p_inc = iio_buffer_step(rxbuf);
			p_samps = (p_end-p_dat)/p_inc;

			cs16_to_cf32(cfibuff, p_dat, p_samps, R_TO_M);
			cbuffercf_write(in1_buffer, cfibuff, p_samps);	
			loop_iter++;
		}
		run = true;

		if (loop_iter > 100 && !in_tx) {
			if (tdma_get_slot(tdma,0)->state == rx_sync) {
                tdma_start_tx(tdma,1);
                in_tx = true;
                printf("Starting TX, slot 1\n");
            } else if (tdma_get_slot(tdma,1)->state == rx_sync) {
                tdma_start_tx(tdma,0);
                in_tx = true;
                printf("Starting TX, slot 0\n");
            }
		}

	    while (run) {
			if (cbuffercf_size(in1_buffer) < (nin * rate_decim)) {
				run = false;
				continue;
			}
			size_t nread_1b;
			float complex * i1b;
			cbuffercf_read(in1_buffer, nin * rate_decim, &i1b, &nread_1b);
			nco_crcf_mix_block_down(downmixer, i1b, rxdc1, nread_1b);
			cbuffercf_release(in1_buffer, nread_1b);
			iirdecim_crcf_execute_block(iir_dc, rxdc1, nin, rxtdma);
			tdma_rx(tdma,(COMP*)rxtdma,rx_samp_count);
			rx_samp_count += nin;

		}
		if(loop_iter > 1000) {
			break;
		}
		
	} 

	tts.tx_quit = true;
	pthread_join(tx_thread, NULL);
	
	iio_buffer_destroy(rxbuf);
 
	iio_context_destroy(ctx_rx);
	iio_context_destroy(ctx_tx);

	free(rxdc1);
	free(rxtdma);

	cbuffercf_destroy(in1_buffer);
	nco_crcf_destroy(downmixer);
	iirdecim_crcf_destroy(iir_dc);

	ttf_destroy(ttf);
	tdma_destroy(tdma);

}

