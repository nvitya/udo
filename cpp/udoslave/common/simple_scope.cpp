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
 *  file:     simple_scope.cpp
 *  brief:    A Simple Scope for UDO Slaves
 *  created:  2023-05-13
 *  authors:  nvitya
*/

#include "simple_scope.h"

#include "stdint.h"
#include "string.h"
#include "udoslave.h"
#include "simple_partable.h"
#include "udoslave_traces.h"

TScope g_scope;

void TScope::Init(uint8_t * abuffer, uint32_t abuffer_size)
{
	pbuffer = abuffer;
	buffer_size = abuffer_size;

	int i;
	for (i = 0; i < SCOPE_MAX_CHANNELS; ++i)
	{
		TScopeChannelData * ch = &channels[i];
		ch->datadef = 0;
		ch->varptr = nullptr;
		ch->bytelen = 0;
	}
}

void TScope::Run() // Called from the idle cycle
{
	if (cmd_prev == cmd)
	{
		return;
	}

	// check for scope start

	if ((state & 0x07) == 0) // idle or ready
	{
		if (cmd & 1)
		{
			// start the scope.
			PrepareSampling();
			if (channel_count > 0)
			{
				state = SCOPE_STATE_PREFILL;
			}
		}
	}

	cmd_prev = cmd;
}

bool TScope::pfn_scope_def(TUdoRequest * udorq, TParameterDef * pdef, void * varptr)
{
	unsigned chnum = udorq->index - 0x5020;

	if (chnum >= SCOPE_MAX_CHANNELS)
	{
		return udo_response_error(udorq, UDOERR_INDEX);
	}

	if (!udorq->iswrite)
	{
		return param_handle_pdef_var(udorq, pdef, varptr);
	}

	// check the write

	uint32_t value = uint32_t(udorq_intvalue(udorq));
	if (!SetChannelDef(chnum, value))
	{
		return udo_response_error(udorq, UDOERR_WRITE_VALUE);
	}

	return true;
}

bool TScope::pfn_scope_cmd(TUdoRequest * udorq, TParameterDef * pdef, void * varptr)
{
	if (!param_handle_pdef_var(udorq, pdef, varptr))
	{
		return false;
	}

	// check the write

	if (udorq->iswrite)
	{
		cmd_prev = 0; // so that the start with SDO will be noticed always
	}

	return true;
}

bool TScope::pfn_scope_data(TUdoRequest * udorq, TParameterDef * pdef, void * varptr)
{
	if (udorq->iswrite)
	{
		return udo_response_error(udorq, UDOERR_READ_ONLY);
	}

  unsigned fullsize = sample_count * sample_width;
  if (0xFFFFFFFF == udorq->offset)  // special offset, return the data length
  {
  	return udo_ro_int(udorq, fullsize, 4);
  }

  //TRACE("scope_data(%u / %u)\n", udorq->offset, fullsize);

	if (fullsize <= udorq->offset)
	{
		udorq->anslen = 0; // empty read: no more data
		return true;
	}

  unsigned remaining = fullsize - udorq->offset;
  if (remaining > udorq->maxanslen)  remaining = udorq->maxanslen;
  udorq->anslen = remaining;

  uint8_t * psrc = next_smp_ptr;  // the first sample data
  psrc += udorq->offset;
  if (psrc >= buf_end_ptr)  // wrap-around handling
  {
  	psrc = pbuffer + (psrc - buf_end_ptr);
  }

  uint8_t * pdst = udorq->dataptr;
  while (remaining > 0)
  {
  	*pdst++ = *psrc++;
  	if (psrc >= buf_end_ptr)  psrc = pbuffer;
  	--remaining;
  }

  return udo_response_ok(udorq);
}

bool TScope::SetChannelDef(unsigned achnum, unsigned adef)
{
	TScopeChannelData * pch = &channels[achnum];

	if (adef == 0) // special case to clear
	{
		pch->datadef = 0; // clear this channel
		pch->varptr = nullptr;
		pch->bytelen = 0;
		return true;
	}

	bool result = false;

	uint16_t index    = (adef >> 16);
	uint8_t  dbytelen = (adef & 0x0F);

	TParameterDef * pdef = pdef_get(index);
	if ( pdef
			 && (pdef->var_ptr)              // parameters without variable pointer are not applicable (including constants)
			 && ((pdef->flags & PARF_RW_MASK) != PARF_WRITEONLY)
			 && (dbytelen == pdef_varsize(pdef)) // the specified size must match
		 )
	{
		pch->pdef = pdef;
		pch->datadef = adef;
		pch->bytelen = dbytelen;
		pch->varptr = (uint8_t *)(pdef->var_ptr);
		if (pdef->flags & PARF_PP)  pch->varptr = *((uint8_t * *)pch->varptr); // resolve double pointer
		result = true;
	}
	else
	{
		pch->datadef = 0; // clear this channel
		pch->varptr = nullptr;
		pch->bytelen = 0;
		pch->pdef = nullptr;
	}

	return result;
}

void TScope::PrepareSampling()
{
	int i;

	sample_width = 0;
	channel_count = 0;
	ptrigch = nullptr;

	for (i = 0; i < SCOPE_MAX_CHANNELS; ++i)
	{
		TScopeChannelData * pch = &channels[i];
		if ((pch->datadef != 0) && pch->varptr && pch->bytelen)
		{
			if (trigger_channel == i)
			{
				ptrigch = pch;
			}

			sample_width += pch->bytelen;
			++channel_count;
		}
		else // stop at the first empty / invalid entry
		{
			break;
		}
	}

	if ((channel_count < 1) || (sample_width < 1))
	{
		return;
	}

	if (!ptrigch)
	{
		ptrigch = &channels[0];  // ensure a valid trigger channel
		trigger_channel = 0;
	}

	sample_count = buffer_size / sample_width;
	if ((max_samples > 0) && (max_samples < sample_count))
	{
		sample_count = max_samples;
	}

	presmp_count  = (sample_count * pretrigger_percent) / 100;
	postsmp_count = sample_count - presmp_count;

	next_smp_ptr = pbuffer;
	buf_end_ptr = next_smp_ptr + (sample_width * sample_count);

	cur_smp_index = 0;
  smp_cycle_counter = 0;

	// prepare the trigger, for unsigned comparison

	tr_value_mask = trigger_mask;

	if (ptrigch->datadef & 0x01)  // signed?
	{
		unsigned char bitlen = (ptrigch->datadef & 0x38);
		if (32 == bitlen)
		{
			tr_value_add = 0x80000000;
			tr_value_mask = trigger_mask;
		}
		else if (16 == bitlen)
		{
			tr_value_add = 0xFFFF8000;
			tr_value_mask = (trigger_mask & 0x0000FFFF);
		}
		else
		{
			tr_value_add = 0xFFFFFF80;
			tr_value_mask = (trigger_mask & 0x000000FF);
		}
	}
	else
	{
		tr_value_add = 0;
	}

	tr_value_threshold = uint32_t(trigger_level & tr_value_mask) + tr_value_add;
	prev_tr_value = 0;
}

void TScope::RunIrqTask()
{
	if (   (SCOPE_STATE_WAITTRIG == state) || (SCOPE_STATE_PREFILL == state)
			|| (SCOPE_STATE_POSTFILL == state) || (SCOPE_STATE_PERMREC == state) )
	{
		// run the sampling

		unsigned n;

		++smp_cycle_counter;

		if (SCOPE_CMD_STOP == cmd)
		{
			// stop the sampling
			state = SCOPE_STATE_IDLE;
		}
		else if (smp_cycle_counter >= smp_cycles)
		{
			smp_cycle_counter = 0;
			// store a sample
			for (unsigned i = 0; i < channel_count; ++i)
			{
				TScopeChannelData * pch = &channels[i];
				for (n = 0; n < pch->bytelen; ++n)
				{
					*next_smp_ptr++ = pch->varptr[n];
				}
			}

			if (next_smp_ptr >= buf_end_ptr)
			{
				next_smp_ptr = pbuffer;
			}

			// store the trigger value
			cur_tr_value = 0;
			uint8_t * ptv = (uint8_t *)&cur_tr_value;
			for (n = 0; n < ptrigch->bytelen; ++n)
			{
				*ptv++ = ptrigch->varptr[n];
			}
			cur_tr_value = (cur_tr_value & tr_value_mask) + tr_value_add;  // prepare value for 32 bit unsigned comparison

			// post sample state handling
			switch (state)
			{
			case SCOPE_STATE_PREFILL:  // the sampling starts here
				++cur_smp_index;
				if (cur_smp_index >= presmp_count)
				{
					if (SCOPE_CMD_PERMREC == cmd)
					{
						state = SCOPE_STATE_PERMREC;
					}
					else
					{
						state = SCOPE_STATE_WAITTRIG;
					}
				}
				break;

			case SCOPE_STATE_PERMREC:  // waiting for trigger arming
				if ((SCOPE_CMD_FORCETRIG == cmd) || (SCOPE_CMD_START == cmd))
				{
					state = SCOPE_STATE_WAITTRIG;
				}
				break;

			case SCOPE_STATE_WAITTRIG:
			{
				bool triggered = false;

				if ((SCOPE_CMD_FORCETRIG == cmd) || (0 == trigger_slope))
				{
					triggered = true;
				}
				else
				{
					// trigger values must be evaluated
					switch (trigger_slope)
					{
					  case 1: // rising edge
							if ((tr_value_threshold <= cur_tr_value) && (prev_tr_value < tr_value_threshold))  triggered = true;
					  	break;
					  case 2: // falling edge
							if ((tr_value_threshold >= cur_tr_value) && (prev_tr_value > tr_value_threshold))  triggered = true;
					  	break;
					  case 3: // equal to
							if (cur_tr_value == tr_value_threshold)  triggered = true;
					  	break;
					  case 4: // not equal
							if (cur_tr_value != tr_value_threshold)  triggered = true;
					  	break;
					  case 5: // any edge
							if (
									 ((cur_tr_value <= tr_value_threshold) && (prev_tr_value > tr_value_threshold))
									 ||
									 ((cur_tr_value >= tr_value_threshold) && (prev_tr_value < tr_value_threshold))
						     )
								   triggered = true;
					  	break;
					  case 6: // change to
							if ((cur_tr_value != prev_tr_value) && (cur_tr_value == tr_value_threshold))  triggered = true;
					  	break;
					}
				}

				if (triggered)
				{
					trigger_index = cur_smp_index;
					state = SCOPE_STATE_POSTFILL;
				}

				break;
			}

			case SCOPE_STATE_POSTFILL:
				++cur_smp_index;
				if (cur_smp_index >= sample_count)
				{
					state = SCOPE_STATE_DATAREADY;
				}
				break;
			}

			prev_tr_value = cur_tr_value;
		}
	}
}

