// VoiceCodec.h - IMA ADPCM, the codec seam.
//
// Shared by both clients. The voice service never calls any of this: it relays
// opaque payloads and has no idea what is inside them, which is exactly what
// lets the codec change without touching the service at all.
//
// WHY ADPCM AND NOT OPUS
//
// Opus is the right long-term answer and this interface exists so it can drop
// in. It is not the right STARTING answer here, because before one packet
// could move we would have to source libopus and build it for Win32 x86 plus
// four Android ABIs - a sub-project ahead of any working feature. IMA ADPCM
// is a well-known ~60-line algorithm with no dependency at all, gives 4:1, and
// at 8 kHz mono lands around 34 kbit/s while a key is held. That is telephone
// quality, which is what proximity chat wants anyway.
//
// To switch later: implement these two functions over Opus, change
// VOICE_FRAME_BYTES in VoiceProtocol.h, rebuild all three. Nothing else knows.

#pragma once

#include "VoiceProtocol.h"

// Per-direction codec state. ADPCM is differential - every sample is decoded
// relative to the one before it - so encoder and decoder each carry a
// predictor, and each speaker a listener hears needs its own decoder state.
//
// It is also why the state travels in the frame: a listener who joins mid-
// sentence, or drops a packet, would otherwise decode everything after it as
// noise. Four bytes per frame is a cheap way to make every frame independent.
struct VOICE_CODEC_STATE
{
    short Predictor;
    short StepIndex;

    void Clear()
    {
        this->Predictor = 0;
        this->StepIndex = 0;
    }
};

// 160 samples in -> VOICE_FRAME_BYTES out. Returns bytes written, 0 on bad
// arguments.
//
// pState is the ENCODER's running state and must persist across calls for one
// outgoing stream - ADPCM predicts each sample from the last, so resetting it
// every frame would put a step discontinuity at every frame boundary and the
// result clicks 50 times a second.
//
// The state as it stood BEFORE this frame is what gets written into the
// frame's first four bytes. That is what lets a decoder start anywhere.
int VoiceEncodeFrame(VOICE_CODEC_STATE* pState, const short* pSamples, int iSampleCount, VBYTE* pOut, int iOutMax);

// The reverse. Returns samples written, or 0 if the frame is malformed.
//
// Takes no state: it reads the predictor and step index out of the frame, so
// any frame decodes correctly on its own. A listener who walks into earshot
// mid-sentence, or who drops a packet, gets clean audio from the very next
// frame instead of noise until the speaker stops talking.
int VoiceDecodeFrame(const VBYTE* pIn, int iInLength, short* pSamples, int iSampleMax);
