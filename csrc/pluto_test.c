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

#include "freedv-tdma/tdma.h"
#include "tdma_testframer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

#define R_TO_M 0.0009765625
#define M_TO_R 16384

#define IIO_BUF_SIZE 4096

uint64_t GetTimeStamp() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
}

typedef struct _tdma_stuff_holder{
    float complex *     tx_buffer;
    bool                have_tx;
    int64_t             tx_time;
	struct _tdma_stuff_holder* next;
} tdma_stuff_holder;

typedef struct {
	tdma_stuff_holder* first;
} tx_queue;

int cb_tx_burst(tdma_t * tdma,float complex* samples, size_t n_samples,i64 timestamp,void * cb_data){
    tx_queue * tx_q = (tx_queue*) cb_data;

	size_t nout = tdma_nout(tdma);
	tdma_stuff_holder * tx_stuff = malloc(sizeof(tdma_stuff_holder));
    tx_stuff->have_tx = true;
	tx_stuff->tx_buffer = malloc(sizeof(complex float)*nout);
    tx_stuff->tx_time = timestamp;
	tx_stuff->next = NULL;
    for(int i = 0; i < tdma_nout(tdma); i++){
        samples[i] = samples[i]*.2;
    }
    memcpy(tx_stuff->tx_buffer,samples,sizeof(float complex)*nout);
	tdma_stuff_holder * p = tx_q->first;
	if (p == NULL) {
		tx_q->first = tx_stuff;
	} else {
		while(p->next != NULL) {
			p = p->next;
		}
		p->next = tx_stuff;
	}
	printf("bursting frame\n");
}

bool run_rx_input(struct iio_buffer* rxbuf, struct iio_channel* rx0_i, cbuffercf rxbuf_out);

static size_t cbuffercf_free(cbuffercf buf){
	return cbuffercf_max_size(buf) - cbuffercf_size(buf);
}

int main (int argc, char **argv)
{
	struct iio_device *rx_dev, *tx_dev;
	struct iio_channel *rx0_i, *rx0_q;
	struct iio_channel *tx0_i, *tx0_q;
	struct iio_buffer *rxbuf;
	struct iio_buffer *txbuf;
    
	struct iio_context *ctx;
	struct iio_device *phy;

    const uint64_t rf_center = 910000000; //Center RF frequency of TDMA signal
    const uint64_t rate_bb = 288000;  //Radio Baseband Freq
    const uint64_t rate_mdm = 48000;  //Modem freq
    const int rate_decim = rate_bb/rate_mdm;
    const float f_shift = 45000;
    uint64_t rf_bbf = rf_center - (uint64_t)f_shift;
	int err;

	iirdecim_crcf iir_dc = iirdecim_crcf_create_default(rate_decim, 8);
	iirinterp_crcf iir_uc = iirinterp_crcf_create_default(rate_decim, 8);
    nco_crcf downmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf upmixer = nco_crcf_create(LIQUID_NCO);
    nco_crcf_set_phase(downmixer, 0.0f);
    nco_crcf_set_phase(upmixer, 0.0f);
    nco_crcf_set_frequency(downmixer,2.*M_PI*(f_shift/(float)rate_bb));
    nco_crcf_set_frequency(upmixer,2.*M_PI*(f_shift/(float)rate_bb));

    struct TDMA_MODE_SETTINGS mode = FREEDV_4800T;
    tdma_t * tdma = tdma_create(mode);
    const int nout = tdma_nout(tdma);
	const int nin = tdma_nin(tdma);
    //tdma_set_rx_cb(tdma,cb_rx_frame,NULL);
    //tdma_set_tx_cb(tdma,cb_tx_frame,NULL);
    tdma->tx_multislot_delay = 10;

    tdma_test_framer * ttf = ttf_create(tdma);
    ttf->print_enable = true;
	ttf->tx_enable = true;
	ttf->tx_master = true;
	ttf->tx_id = 101;


	tx_queue tx_bursts;
	tx_bursts.first = NULL;
    tdma_set_tx_burst_cb(tdma,cb_tx_burst,(void*)&tx_bursts);


	cbuffercf in1_buffer = cbuffercf_create(nin*rate_decim+4096);
	cbuffercf out1_buffer = cbuffercf_create(nout*rate_decim+4096);
     
	//ctx = iio_create_context_from_uri("ip:192.168.2.1");
	ctx = iio_create_context_from_uri("local:");

	phy = iio_context_find_device(ctx, "ad9361-phy");
	rx_dev = iio_context_find_device(ctx, "cf-ad9361-lpc");
	tx_dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc");

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
		
	rx0_i = iio_device_find_channel(rx_dev, "voltage0", 0);
	rx0_q = iio_device_find_channel(rx_dev, "voltage1", 0);
 
	iio_channel_enable(rx0_i);
	iio_channel_enable(rx0_q);

	tx0_i = iio_device_find_channel(tx_dev, "voltage0", true);
	tx0_q = iio_device_find_channel(tx_dev, "voltage1", true);

	iio_channel_enable(tx0_i);
	iio_channel_enable(tx0_q);

	rxbuf = iio_device_create_buffer(rx_dev, IIO_BUF_SIZE, false);
	if (!rxbuf) {
		perror("Could not create RX buffer");
		exit(1);
	}

	txbuf = iio_device_create_buffer(tx_dev, IIO_BUF_SIZE, false);

	complex float tx_uc_cf_buf[IIO_BUF_SIZE];
	complex float cfibuff[IIO_BUF_SIZE];
	complex float *rxdc1  = (complex float*) malloc(sizeof(complex float) * nin * rate_decim);
	complex float *rxtdma = (complex float*) malloc(sizeof(complex float) * nin);
	err = iio_buffer_set_blocking_mode(txbuf, false);
	if (err != 0) {
		printf("ERR iio_buffer_set_blocking_mode: %s\n",strerror(-err));
	}
	bool run = true;
	bool tx_redo = false;
	int loop_iter = 0;
	uint64_t rx_samp_count = 0;
	uint64_t tx_bb_samp_count = 0;
	uint64_t tx_samp_count = 0;

	complex float *tx_lb_buf = (complex float*) malloc(sizeof(complex float) * nin * rate_decim);
	complex float *tx_burst_buf = (complex float*) malloc(sizeof(complex float) * nin * rate_decim);
	complex float *tx_burst_ptr = tx_burst_buf;
	uint64_t tx_burst_time = 0;
	size_t tx_burst_n = 0;
	tdma_start_tx(tdma, 1);
	while (true) {
		loop_iter++;
		run = true;
		while(run) {
			void *p_dat, *p_end, *t_dat;
			ptrdiff_t p_inc;

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
			p_inc = iio_buffer_step(rxbuf);
			p_end = iio_buffer_end(rxbuf);
			p_dat = iio_buffer_first(rxbuf, rx0_i);

			int id = 0;

			for (; p_dat < p_end; p_dat += p_inc, t_dat += p_inc) {
				const int16_t i = ((int16_t*)p_dat)[0]; // Real (I)
				const int16_t q = ((int16_t*)p_dat)[1]; // Imag (Q)
	
				// Convert into CF32 and stick on output buffer
				cfibuff[id] = ((float)i)*R_TO_M + ((float)q)*R_TO_M*I;
				id++;
			}
			cbuffercf_write(in1_buffer, cfibuff, IIO_BUF_SIZE);
		}
		run = true;

	    while (run) {
			if (cbuffercf_size(in1_buffer) < (nin * rate_decim)) {
				run = false;
				continue;
			}
			int nread_1b;
			float complex * i1b;
			cbuffercf_read(in1_buffer, nin * rate_decim, &i1b, &nread_1b);
			if (nread_1b < nin * rate_decim) {
				printf("Got %d in instead of %d\n",nread_1b, nin * rate_decim);
			}
			nco_crcf_mix_block_down(downmixer, i1b, rxdc1, nread_1b);
			cbuffercf_release(in1_buffer, nread_1b);
			iirdecim_crcf_execute_block(iir_dc, rxdc1, nin, rxtdma);
			printf("RX'ing slot\n");
			tdma_rx(tdma,(COMP*)rxtdma,rx_samp_count);
			rx_samp_count += nin;

		}

		while (tx_bursts.first != NULL && tx_burst_n == 0) {
			printf("converting frame\n");
			tdma_stuff_holder * tx_stuff = tx_bursts.first;
			tx_bursts.first = tx_stuff->next;
			tx_stuff->have_tx = false;
			if (tx_stuff->tx_time * rate_decim < tx_samp_count) {
				printf("Skipping frame conversion\n");
				continue;
			}
			iirinterp_crcf_execute_block(iir_uc, tx_stuff->tx_buffer, nout, tx_lb_buf);
			nco_crcf_mix_block_up(upmixer, tx_lb_buf, tx_burst_buf, nout * rate_decim);
			cbuffercf_write(out1_buffer, tx_burst_buf, nout * rate_decim);
			tx_burst_time = tx_stuff->tx_time * rate_decim;
			
			tx_burst_ptr = tx_burst_buf;
			tx_burst_n = nout * rate_decim;
				printf("nbt: %lld rbt:%lld ts:%lld\n", tx_burst_time, rx_samp_count * rate_decim, tx_samp_count);
		}

		run = true;
		while (run) {
			void *p_dat, *p_end, *t_dat;
			ptrdiff_t p_inc;
			
			p_inc = iio_buffer_step(txbuf);
			p_end = iio_buffer_end(txbuf);
			p_dat = iio_buffer_first(txbuf, tx0_i);
			int p_samps = (p_end - p_dat) / p_inc;
			if (tx_burst_time == 0) {
				memset(p_dat, 0, p_end - p_dat);
			} else if (tx_samp_count > tx_burst_time) { // We missed the TX burst
				tx_burst_time = 0;
				tx_burst_n = 0;
				memset(p_dat, 0, p_end - p_dat);
				printf("Burst missed!\n");
				cbuffercf_reset(out1_buffer);
				memset(p_dat, 0, p_end - p_dat);
			} else if ( cbuffercf_size(out1_buffer) > 1 && (tx_samp_count + p_samps) > tx_burst_time ) {
				memset(p_dat, 0, p_end - p_dat);
				uint64_t start_off = tx_burst_time - tx_samp_count;
				uint64_t n_burst_samps = p_samps - start_off;
				float complex * buf;
				int nread_tx;
				cbuffercf_read(out1_buffer, n_burst_samps, &buf, &nread_tx);
				cbuffercf_release(out1_buffer, nread_tx);
				p_dat += p_inc * start_off;
				for (int i = 0; i < nread_tx; p_dat += p_inc, buf++, i++) {
					assert(p_dat <= p_end);
					((int16_t*)p_dat)[0] = (int16_t)(crealf(*buf)*M_TO_R);
					((int16_t*)p_dat)[1] = (int16_t)(cimagf(*buf)*M_TO_R);
				}
				tx_burst_time += nread_tx;
				tx_burst_n -= nread_tx;
			} else if (cbuffercf_size(out1_buffer) == 0) {
				cbuffercf_reset(out1_buffer);
				tx_burst_time = 0;
				tx_burst_n = 0;
			}


			int ret = iio_buffer_push(txbuf);
			// Can we send outgoing samps
			if(ret == -EAGAIN) {
				run = false;
				do {
					ret = iio_buffer_push(txbuf);
				} while( ret == -EAGAIN);
				
			} else {
				tx_redo = false;
			}
			tx_samp_count += p_samps;
		}

		
	} 
    // firdecim_crcf_destroy(fir_dc);
    // firinterp_crcf_destroy(fir_uc);
	iio_buffer_destroy(rxbuf);
 
	iio_context_destroy(ctx);
 
}
