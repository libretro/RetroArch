/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  run_loop.c
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#include <btstack/run_loop.h>

#include <stdio.h>
#include <stdlib.h>  // exit()

#include "run_loop_private.h"

#include "debug.h"
#include "config.h"

static run_loop_t * the_run_loop = NULL;

extern const run_loop_t run_loop_embedded;

#ifdef USE_POSIX_RUN_LOOP
extern run_loop_t run_loop_posix;
#endif

#ifdef USE_COCOA_RUN_LOOP
extern run_loop_t run_loop_cocoa;
#endif

// assert run loop initialized
void run_loop_assert(void){
#ifndef EMBEDDED
    if (!the_run_loop){
        log_error("ERROR: run_loop function called before run_loop_init!\n");
        exit(10);
    }
#endif
}

/**
 * Add data_source to run_loop
 */
void run_loop_add_data_source(data_source_t *ds){
    run_loop_assert();
    the_run_loop->add_data_source(ds);
}

/**
 * Remove data_source from run loop
 */
int run_loop_remove_data_source(data_source_t *ds){
    run_loop_assert();
    return the_run_loop->remove_data_source(ds);
}

/**
 * Add timer to run_loop (keep list sorted)
 */
void run_loop_add_timer(timer_source_t *ts){
    run_loop_assert();
    the_run_loop->add_timer(ts);
}

/**
 * Remove timer from run loop
 */
int run_loop_remove_timer(timer_source_t *ts){
    run_loop_assert();
    return the_run_loop->remove_timer(ts);
}

void run_loop_timer_dump(){
    run_loop_assert();
    the_run_loop->dump_timer();
}

/**
 * Execute run_loop
 */
void run_loop_execute() {
    run_loop_assert();
    the_run_loop->execute();
}

// init must be called before any other run_loop call
void run_loop_init(RUN_LOOP_TYPE type){
#ifndef EMBEDDED
    if (the_run_loop){
        log_error("ERROR: run loop initialized twice!\n");
        exit(10);
    }
#endif
    switch (type) {
#ifdef EMBEDDED
        case RUN_LOOP_EMBEDDED:
            the_run_loop = (run_loop_t*) &run_loop_embedded;
            break;
#endif
#ifdef USE_POSIX_RUN_LOOP
        case RUN_LOOP_POSIX:
            the_run_loop = &run_loop_posix;
            break;
#endif
#ifdef USE_COCOA_RUN_LOOP
        case RUN_LOOP_COCOA:
            the_run_loop = &run_loop_cocoa;
            break;
#endif
        default:
#ifndef EMBEDDED
            log_error("ERROR: invalid run loop type %u selected!\n", type);
            exit(10);
#endif
            break;
    }
    the_run_loop->init();
}

