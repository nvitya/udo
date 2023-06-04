/* -----------------------------------------------------------------------------
 * This file is a part of the UDO project: https://github.com/nvitya/udo
 * Copyright (c) 2023 Viktor Nagy, nvitya
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software. Permission is granted to anyone to use this
 * software for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 * --------------------------------------------------------------------------- */
/*
 *  file:     scope.h
 *  brief:    A Simple Scope for UDO Slaves
 *  created:  2023-05-13
 *  authors:  nvitya
*/

#ifndef SIMPLE_SCOPE_H_
#define SIMPLE_SCOPE_H_

#define SCOPE_VERSION   (1 * 1000000 + 0 * 1000 + 0)

#define SCOPE_MAX_CHANNELS      8

#define SCOPE_STATE_IDLE        0
#define SCOPE_STATE_PREFILL     1
#define SCOPE_STATE_PERMREC     3
#define SCOPE_STATE_WAITTRIG    5
#define SCOPE_STATE_POSTFILL    7
#define SCOPE_STATE_DATAREADY   8

#define SCOPE_CMD_STOP          0
#define SCOPE_CMD_PERMREC       1
#define SCOPE_CMD_START         3
#define SCOPE_CMD_FORCETRIG     7

#include "udoslave.h"
#include "simple_partable.h"

typedef struct TScopeChannelData
{
	unsigned         datadef;
	uint32_t         bytelen;
	uint8_t *	       varptr;
	TParameterDef *  pdef;
//
} TScopeChannelData;

class TScope : public TClass  // for parameter callbacks TClass base is required
{
protected:  // internals
	uint8_t             channel_count = 0;  // will be calculated in PrepareSampling()
	TScopeChannelData * ptrigch = nullptr;

	uint8_t *           next_smp_ptr;
	uint8_t *           buf_end_ptr;

	// trigger

	unsigned            tr_value_add;
	unsigned            tr_value_threshold;
	unsigned            tr_value_mask;

	unsigned            triggercomparevalue;

	unsigned            cur_tr_value;
	unsigned            prev_tr_value;


	uint16_t            smp_cycle_counter = 0;

	unsigned 						presmp_count;
	unsigned 						postsmp_count;

	unsigned            cur_smp_index;

public:
	uint8_t *           pbuffer = nullptr;
	uint32_t            buffer_size = 0;

public:  // state + cmd
	uint8_t             state    = SCOPE_STATE_IDLE;
	uint8_t             cmd      = SCOPE_CMD_STOP;
	uint8_t             cmd_prev = SCOPE_CMD_STOP;

	uint32_t 						sample_count = 0;
	uint32_t 						sample_width = 0;
	uint32_t            trigger_index = 0;

public: // configuration
	uint8_t             trigger_channel = 0;
	uint8_t             trigger_slope = 0;
	uint8_t             pretrigger_percent = 50;
	uint16_t            smp_cycles = 1;
	uint32_t            max_samples = 0;  // 0 =
	int32_t             trigger_level = 0;
	uint32_t            trigger_mask = 0xFFFFFFFF;

	TScopeChannelData		channels[SCOPE_MAX_CHANNELS];


public:


public:
	void Init(uint8_t * abuffer, uint32_t abuffer_size);

	void Run();
	void RunIrqTask();

	bool SetChannelDef(unsigned achnum, unsigned adef);

	bool pfn_scope_data(TUdoRequest * udorq, TParameterDef * pdef, void * varptr);

	void PrepareSampling();

	bool pfn_scope_def(TUdoRequest * udorq, TParameterDef * pdef, void * varptr);
	bool pfn_scope_cmd(TUdoRequest * udorq, TParameterDef * pdef, void * varptr);
};

extern TScope g_scope;

#endif /* SIMPLE_SCOPE_H_ */
