// VoiceCodec.cpp - IMA ADPCM.
//
// See VoiceCodec.h for why this and not Opus, and how to swap it later.
//
// FRAME LAYOUT (VOICE_FRAME_BYTES = 84)
//
//   [0..1]  predictor, signed 16-bit little endian, state BEFORE this frame
//   [2]     step table index, 0..88
//   [3]     reserved, always 0 - keeps the payload 2-byte aligned and leaves
//           somewhere to put a codec id if this is ever swapped without
//           changing the frame size
//   [4..83] 160 samples, one 4-bit code each, low nibble first
//
// The state travelling in every frame is what makes frames independent. It
// costs 4 bytes in 84 - under 5% - and buys a listener who joins mid-sentence
// clean audio from the next frame instead of noise.

#include "VoiceCodec.h"

#include <string.h>

// The two tables the IMA ADPCM standard is built on. Not tunable: a decoder
// using different ones produces noise, and the frames carry no table id.
static const int g_StepTable[89] =
{
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
	19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
	50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
	130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
	337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
	2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

// How far up or down the step table each code moves. Small codes mean the
// signal is quiet and the step should shrink; large codes mean it is moving
// fast and the step should grow. This adaptation is the whole trick - it is
// what lets 4 bits carry a 16-bit sample.
static const int g_IndexTable[16] =
{
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};

static inline int ClampSample(int value)
{
	if (value > 32767)
	{
		return 32767;
	}

	if (value < -32768)
	{
		return -32768;
	}

	return value;
}

static inline int ClampIndex(int index)
{
	if (index < 0)
	{
		return 0;
	}

	if (index > 88)
	{
		return 88;
	}

	return index;
}

int VoiceEncodeFrame(VOICE_CODEC_STATE* pState, const short* pSamples, int iSampleCount, VBYTE* pOut, int iOutMax)
{
	if (pState == 0 || pSamples == 0 || pOut == 0)
	{
		return 0;
	}

	if (iSampleCount != VOICE_FRAME_SAMPLES || iOutMax < VOICE_FRAME_BYTES)
	{
		return 0;
	}

	int predictor = pState->Predictor;
	int index = ClampIndex(pState->StepIndex);

	// Written before encoding, so it describes the state the decoder must
	// start from - not the state left behind afterwards.
	pOut[0] = (VBYTE)(predictor & 0xFF);
	pOut[1] = (VBYTE)((predictor >> 8) & 0xFF);
	pOut[2] = (VBYTE)index;
	pOut[3] = 0;

	VBYTE* pPayload = pOut + 4;

	memset(pPayload, 0, VOICE_FRAME_BYTES - 4);

	for (int n = 0; n < iSampleCount; n++)
	{
		const int step = g_StepTable[index];

		int diff = (int)pSamples[n] - predictor;
		int code = 0;

		if (diff < 0)
		{
			code = 8;          // sign bit
			diff = -diff;
		}

		// Three magnitude bits, each worth half the last - a binary search
		// over the step rather than a divide.
		int magnitude = step;

		if (diff >= magnitude)
		{
			code |= 4;
			diff -= magnitude;
		}

		magnitude >>= 1;

		if (diff >= magnitude)
		{
			code |= 2;
			diff -= magnitude;
		}

		magnitude >>= 1;

		if (diff >= magnitude)
		{
			code |= 1;
		}

		// The encoder now decodes its own code and keeps THAT as the
		// predictor, not the true sample. If it tracked the real signal the
		// two sides would drift apart, because the decoder only ever sees the
		// quantised version.
		int diffq = step >> 3;

		if (code & 4)
		{
			diffq += step;
		}

		if (code & 2)
		{
			diffq += step >> 1;
		}

		if (code & 1)
		{
			diffq += step >> 2;
		}

		if (code & 8)
		{
			predictor -= diffq;
		}
		else
		{
			predictor += diffq;
		}

		predictor = ClampSample(predictor);

		index = ClampIndex(index + g_IndexTable[code]);

		// Low nibble first, matching the decoder below.
		if ((n & 1) == 0)
		{
			pPayload[n >> 1] = (VBYTE)(code & 0x0F);
		}
		else
		{
			pPayload[n >> 1] |= (VBYTE)((code & 0x0F) << 4);
		}
	}

	pState->Predictor = (short)predictor;
	pState->StepIndex = (short)index;

	return VOICE_FRAME_BYTES;
}

int VoiceDecodeFrame(const VBYTE* pIn, int iInLength, short* pSamples, int iSampleMax)
{
	if (pIn == 0 || pSamples == 0)
	{
		return 0;
	}

	if (iInLength < VOICE_FRAME_BYTES || iSampleMax < VOICE_FRAME_SAMPLES)
	{
		return 0;
	}

	int predictor = (short)((VWORD)pIn[0] | ((VWORD)pIn[1] << 8));
	int index = pIn[2];

	// A corrupt or hostile frame could carry any index. Left unchecked it
	// would read past g_StepTable on the very first sample.
	if (index < 0 || index > 88)
	{
		return 0;
	}

	const VBYTE* pPayload = pIn + 4;

	for (int n = 0; n < VOICE_FRAME_SAMPLES; n++)
	{
		const int code = ((n & 1) == 0)
			? (pPayload[n >> 1] & 0x0F)
			: ((pPayload[n >> 1] >> 4) & 0x0F);

		const int step = g_StepTable[index];

		int diffq = step >> 3;

		if (code & 4)
		{
			diffq += step;
		}

		if (code & 2)
		{
			diffq += step >> 1;
		}

		if (code & 1)
		{
			diffq += step >> 2;
		}

		if (code & 8)
		{
			predictor -= diffq;
		}
		else
		{
			predictor += diffq;
		}

		predictor = ClampSample(predictor);

		index = ClampIndex(index + g_IndexTable[code]);

		pSamples[n] = (short)predictor;
	}

	return VOICE_FRAME_SAMPLES;
}
