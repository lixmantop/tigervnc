/* Copyright 2022 Mikhail Kupchik
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#include "PulseAudioOutput.h"
#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pulse/pulseaudio.h>

#define FORMAT PA_SAMPLE_S16LE
#define RATE 44100

char *ring_ptr;
size_t  ring_size;
size_t ring_head;
size_t ring_last;
int underflows =0;
int latency = 60000;
pa_sample_spec sample_specifications;
pa_buffer_attr buffer_attr;
pa_stream *stream =NULL;
pa_threaded_mainloop *mainloop;
pa_context *context=NULL;
void context_state_cb(pa_context* context, void* mainloop);
void stream_state_cb(pa_stream *s, void *mainloop);
void stream_success_cb(pa_stream *stream, int success, void *userdata);
void stream_write_cb(pa_stream *stream_, size_t requested_bytes, void *userdata);
void stream_underflow_cb(pa_stream *s, void *userdata);

void ring_init(char *buffer, size_t size){
	ring_ptr=buffer;
	ring_size =size;
	ring_head=0;
	ring_last= 0;
}


int getSizeInBuffer() {
	// size data in buffer
	if(ring_last>ring_head ) {
		return ring_last-ring_head;
	}else if(ring_last<ring_head ) {
		return ring_size-ring_head+ ring_last;
	}
	return 0;
}
int ring_push(char *buf, size_t size) {
	if((size+(size_t)getSizeInBuffer())>(ring_size-1)) {
		//printf("error empty space buffer to save =%ld \n",size-(ring_head-ring_last));
		return 1;
	}
	if(ring_last>=ring_head && ring_size-ring_last>=size  ) {
		memcpy(ring_ptr+ring_last,buf,size);
		ring_last+=size;
	} else if(ring_last>=ring_head && ring_last+size>ring_size){
			memcpy(ring_ptr+ring_last,buf,ring_size-ring_last);
			memcpy(ring_ptr,buf +(ring_size-ring_last),size-(ring_size-ring_last));
			ring_last = size-(ring_size-ring_last);
	} else if(ring_last<ring_head && ring_head-ring_last>=size){
		memcpy(ring_ptr+ring_last,buf,size);
		ring_last += size;
	} else if(ring_last<ring_head && (ring_head-ring_last)<size){
	//	printf("error empty space buffer1 save =%ld \n",size-(ring_head-ring_last));
		return 1;
	}
	return 0;
}
int ring_pop(char *buf, size_t size) {

	if(size>(size_t)getSizeInBuffer()) {
		size=getSizeInBuffer();
	}

	if(ring_last>=ring_head && ring_last-ring_head>=size  ) {
		memcpy(buf,ring_ptr+ring_head,size);
		ring_head+=size;
		return size;
	} else if(ring_last<=ring_head && ring_head+size>ring_size){
			memcpy(buf,ring_ptr+ring_head,ring_size-ring_head);
			memcpy(buf +(ring_size-ring_head),ring_ptr,size-(ring_size-ring_head));
			ring_head = size-(ring_size-ring_head);
			return size;
	} else if(ring_last<ring_head && ring_head-ring_size>=size){
		memcpy(buf ,ring_ptr+ring_head,size);
		ring_head += size;
		return size;
	} else if(ring_last<ring_head && ((ring_size-ring_head) + ring_last)<size){
		//printf("error No data buffer to load =%ld \n",size-((ring_size-ring_head) + ring_last));
		return -1;
	}
	return 0;
}


//void test() {
//
//	char *buf = (char *)malloc(20);
//	ring_init(buf,20);
//	char refin[3000]="VNSDVNVDWNSDVNQNVZSFVNZSEFHVQVBSJHBVHJKBSVLHQIUSVFHIUQZHFZHEfiuhZUEIFHIUHZEFiHZEIFBIZEBVFiBEVICFUIEUICFIUEZCIUCGBIEGBcBCIBcbUIBCUIBEUICbIUCIBIcbIBCIBCiBCIVISVBQSICBVFGBGFYGFIZEFIZIUFIZEFgiZGFIZGfigZFVUZEIfuizEFIZfBHVCBSDHVBJHSVBHJQSBVLJBScvbcbMCFBFBchqfcxghygtuhzserdjhbsd ncsdfZHVNSDVNVDWNSDVNQNVZSFVNZSEFHVQVBSJHBVHJKBSVLHQIUSVFHIUQZHFZHEfiuhZUEIFHIUHZEFiHZEIFBIZEBVFiBEVICFUIEUICFIUEZCIUCGBIEGBcBCIBcbUIBCUIBEUICbIUCIBIcbIBCIBCiBCIVISVBQSICBVFGBGFYGFIZEFIZIUFIZEFgiZGFIZGfigZFVUZEIfuizEFIZfBHVCBSDHVBJHSVBHJQSBVLJBScvbcbMCFBFBchqfcxghygtuhzserdjhbsd ncsdfZHVNSDVNVDWNSDVNQNVZSFVNZSEFHVQVBSJHBVHJKBSVLHQIUSVFHIUQZHFZHEfiuhZUEIFHIUHZEFiHZEIFBIZEBVFiBEVICFUIEUICFIUEZCIUCGBIEGBcBCIBcbUIBCUIBEUICbIUCIBIcbIBCIBCiBCIVISVBQSICBVFGBGFYGFIZEFIZIUFIZEFgiZGFIZGfigZFVUZEIfuizEFIZfBHVCBSDHVBJHSVBHJQSBVLJBScvbcbMCFBFBchqfcxghygtuhzserdjhbsd ncsdfZHVNSDVNVDWNSDVNQNVZSFVNZSEFHVQVBSJHBVHJKBSVLHQIUSVFHIUQZHFZHEfiuhZUEIFHIUHZEFiHZEIFBIZEBVFiBEVICFUIEUICFIUEZCIUCGBIEGBcBCIBcbUIBCUIBEUICbIUCIBIcbIBCIBCiBCIVISVBQSICBVFGBGFYGFIZEFIZIUFIZEFgiZGFIZGfigZFVUZEIfuizEFIZfBHVCBSDHVBJHSVBHJQSBVLJBScvbcbMCFBFBchqfcxghygtuhzserdjhbsd ncsdf";
//	int indexin=0;
//	char refout[3000]="";
//	int indexout=0;
//
//	while(indexin<2000) {
//		ring_push(refin+indexin,10);
//		indexin+=10;
//
//		ring_pop(refout+indexout,5);
//		indexout+=5;
//
//		ring_push(refin+indexin,10);
//		indexin+=10;
//
//		ring_push(refin+indexin,2);
//		indexin+=2;
//
//		ring_pop(refout+indexout,17);
//		indexout+=17;
//
//	}
//
//	for(int i=0;i<2000;i++) {
//		if(strncmp(refin+i,refout+i,1)!=0) {
//			printf("i=%d \n",i);
//		}
//	}
//
//
//}

PulseAudioOutput::PulseAudioOutput():
init(false)
{
	size_t buf_alloc_size = 1;
	 while (buf_alloc_size < 192000)
	   buf_alloc_size <<= 1;


	size_buffer= buf_alloc_size*4;
	buffer = ((char*)( calloc(buf_alloc_size, 4) ));
	ring_init(buffer,size_buffer);

	// https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/Samples/AsyncPlayback/
	pa_mainloop_api *mainloop_api;

	// Get a mainloop and its context
	mainloop = pa_threaded_mainloop_new();
	assert(mainloop);
	mainloop_api = pa_threaded_mainloop_get_api(mainloop);
	context = pa_context_new(mainloop_api, "vnc");
	assert(context);

	// Set a callback so we can wait for the context to be ready
	pa_context_set_state_callback(context, &context_state_cb, mainloop);

	// Lock the mainloop so that it does not run and crash before the context is ready
	pa_threaded_mainloop_lock(mainloop);

	// Start the mainloop
	assert(pa_threaded_mainloop_start(mainloop) == 0);
	if(pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) != 0) {
		init = false;
		return;
	}

	// Wait for the context to be ready
	for(;;) {
		pa_context_state_t context_state = pa_context_get_state(context);
		assert(PA_CONTEXT_IS_GOOD(context_state));
		if (context_state == PA_CONTEXT_READY) break;
		pa_threaded_mainloop_wait(mainloop);
	}

	// Create a playback stream
	//pa_sample_spec sample_specifications;
	sample_specifications.format = FORMAT;
	sample_specifications.rate = RATE;
	sample_specifications.channels = 2;

	pa_channel_map map;
	pa_channel_map_init_stereo(&map);

	stream = pa_stream_new(context, "Playback", &sample_specifications, &map);
	pa_stream_set_state_callback(stream, stream_state_cb, mainloop);
	pa_stream_set_write_callback(stream, stream_write_cb, mainloop);
	//  pa_stream_set_underflow_callback(stream, stream_underflow_cb, mainloop);

	// recommended settings, i.e. server uses sensible values
	//pa_buffer_attr buffer_attr;
	buffer_attr.maxlength = pa_usec_to_bytes(latency,&sample_specifications);
	buffer_attr.tlength = pa_usec_to_bytes(latency,&sample_specifications);
	buffer_attr.prebuf = (uint32_t) -1;
	buffer_attr.minreq = (uint32_t) -1;

	// Settings copied as per the chromium browser source
	pa_stream_flags_t stream_flags;
	stream_flags = (pa_stream_flags_t)(PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING |
			PA_STREAM_NOT_MONOTONIC | PA_STREAM_AUTO_TIMING_UPDATE |
			PA_STREAM_ADJUST_LATENCY);

	// Connect stream to the default audio output sink
	assert(pa_stream_connect_playback(stream, NULL, &buffer_attr, stream_flags, NULL, NULL) == 0);

	// Wait for the stream to be ready
	for(;;) {
		pa_stream_state_t stream_state = pa_stream_get_state(stream);
		assert(PA_STREAM_IS_GOOD(stream_state));
		if (stream_state == PA_STREAM_READY) break;
		pa_threaded_mainloop_wait(mainloop);
	}

	pa_threaded_mainloop_unlock(mainloop);
	init=true;
}

void context_state_cb([[maybe_unused]]pa_context* context_, void* mainloop_) {
    pa_threaded_mainloop_signal((pa_threaded_mainloop *)mainloop_, 0);
}

void stream_state_cb([[maybe_unused]]pa_stream *s, void *mainloop_) {
    pa_threaded_mainloop_signal((pa_threaded_mainloop *)mainloop_, 0);
}

void stream_write_cb(pa_stream *stream_, size_t requested_bytes, [[maybe_unused]]void *userdata) {
	uint8_t *buffer = NULL;
	size_t size = getSizeInBuffer();

	if(size>0) {
		pa_stream_begin_write(stream_, (void**) &buffer, &requested_bytes);
		ring_pop((char *)buffer,requested_bytes);
		pa_stream_write(stream_, buffer, requested_bytes, NULL, 0LL, PA_SEEK_RELATIVE);
	} else {
		// white noise
		pa_stream_begin_write(stream_, (void**) &buffer, &requested_bytes);
		memset(buffer, 0, requested_bytes);
		pa_stream_write(stream_, buffer, requested_bytes, NULL, 0LL, PA_SEEK_RELATIVE);
	}
}
void stream_underflow_cb([[maybe_unused]]pa_stream *s, [[maybe_unused]]void *userdata) {
  printf("underflow\n");
}

void stream_success_cb([[maybe_unused]]pa_stream *stream_,[[maybe_unused]] int success, [[maybe_unused]]void *userdata) {
    return;
}

size_t PulseAudioOutput::addSamples(const uint8_t* data, size_t size) {
	if(init) {
		ring_push((char *)data,size);
	}
	return size;
}


bool PulseAudioOutput::submitSamples() {
return true;
}

void PulseAudioOutput::cleanBuffer() {
	ring_last=ring_head=0;
	pa_threaded_mainloop_lock(mainloop);
	pa_stream_flush(stream, stream_success_cb, NULL);
	pa_threaded_mainloop_unlock(mainloop);
}

void PulseAudioOutput::notifyStreamingStartStop(bool isStart) {
	if(init) {
		if(isStart) {
			pa_threaded_mainloop_lock(mainloop);
			pa_stream_cork(stream, 0, stream_success_cb, mainloop);
			pa_threaded_mainloop_unlock(mainloop);
		} else {
			pa_threaded_mainloop_lock(mainloop);
			pa_stream_cork(stream, 1, stream_success_cb, mainloop);
			pa_threaded_mainloop_unlock(mainloop);
		}
	}
}


PulseAudioOutput::~PulseAudioOutput()
{
	// https://chromium.googlesource.com/chromium/src/media/+/refs/heads/main/audio/pulse/pulse_output.cc
	if(init) {
		notifyStreamingStartStop(false);
		pa_operation* operation;
		if(stream){
		 operation = pa_stream_flush(stream, NULL, mainloop);
			while(pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
				pa_threaded_mainloop_wait(mainloop);
			}
			pa_stream_disconnect(stream);
			pa_stream_set_write_callback(stream,NULL,NULL);
			pa_stream_set_state_callback(stream,NULL,NULL);
			pa_stream_unref(stream);
			stream=NULL;
		}
		if(context){
			pa_context_disconnect(context);
			pa_context_set_state_callback(context,NULL, NULL);
			pa_context_unref(context);
			context=NULL;
		}
		if(mainloop) {
			pa_threaded_mainloop_stop(mainloop);
			pa_threaded_mainloop_free(mainloop);
			mainloop =NULL;
		}
	}
	if(buffer != NULL) {
		free(buffer);
	}
}

