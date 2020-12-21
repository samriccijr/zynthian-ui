/*
 * ******************************************************************
 * ZYNTHIAN PROJECT: Zynseq Library
 *
 * Library providing step sequencer as a Jack connected device
 *
 * Copyright (C) 2020 Brian Walton <brian@riban.co.uk>
 *
 * ******************************************************************
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE.txt file.
 *
 * ******************************************************************
 */

#include <stdio.h> //provides printf
#include <stdlib.h> //provides exit
#include <thread> //provides thread for timer
#include <chrono> //provides timespan for timer
#include "patternmanager.h" //provides management of patterns and sequences
#include "timebase.h" //provides timebase event map
#include <jack/jack.h> //provides JACK interface
#include <jack/midiport.h> //provides JACK MIDI interface
#include "zynseq.h" //exposes library methods as c functions

#define DPRINTF(fmt, args...) if(g_bDebug) printf(fmt, ## args)

Pattern* g_pPattern = 0; // Currently selected pattern
uint32_t g_nPattern = 0; // Index of currently selected pattern
jack_port_t * g_pInputPort; // Pointer to the JACK input port
jack_port_t * g_pOutputPort; // Pointer to the JACK output port
jack_client_t *g_pJackClient = NULL; // Pointer to the JACK client

jack_nframes_t g_nSampleRate = 44100; // Quantity of samples per second
std::map<uint32_t,MIDI_MESSAGE*> g_mSchedule; // Schedule of MIDI events (queue for sending), indexed by scheduled play time (samples since JACK epoch)
bool g_bDebug = false; // True to output debug info
uint32_t g_nSongPosition = 0; // Clocks since start of song
uint32_t g_nSongLength = 0; // Clocks cycles to end of song
bool g_bModified = false; // True if pattern has changed since last check
bool g_bPlaying = false; // True if any sequence in current song is playing

uint8_t g_nInputChannel = 1; // MIDI input channel (>15 to disable MIDI input)

uint8_t g_nSongStatus = STOPPED; // Status of song (not other sequences)
bool g_bMutex = false; // Mutex lock for access to g_mSchedule

// Tranpsort variables apply to next period
float g_fBeatsPerBar = 4.0;
float g_fBeatType = 4.0;
double g_dTicksPerBeat = 1920.0;
double g_dTempo = 120.0;
double g_dTicksPerClock = g_dTicksPerBeat / 24;
bool g_bTimebaseChanged = false;
Timebase* g_pTimebase = NULL; // Pointer to the timebase object for selected song
TimebaseEvent* g_pNextTimebaseEvent = NULL; // Pointer to the next timebase event or NULL if no more events in this song
uint32_t g_nBar = 1; // Current bar
uint32_t g_nBeat = 1; // Current beat within bar
uint32_t g_nTick = 0; // Current tick within bar
double g_dBarStartTick = 0; // Quantity of ticks from start of song to start of current bar
jack_nframes_t g_nFramesToNextClock = 0; // Quantity of frames from start of next period to next clock pulse (24 clocks per beat, 1920/24=80 tickst per clock. Depends on tempo and sample rate)
jack_nframes_t g_nTransportStartFrame = 0; // Quantity of frames from JACK epoch to transport start
double g_dFramesPerClock;
uint8_t g_nClock = 0; // Quantity of MIDI clocks since start of beat

// ** Internal (non-public) functions  (not delcared in header so need to be in correct order in source file) **

// Enable / disable debug output
void enableDebug(bool bEnable)
{
    printf("libseq setting debug mode %s\n", bEnable?"on":"off");
    g_bDebug = bEnable;
}

// Convert tempo to frames per tick
double getFramesPerTick(double dTempo)
{
    //!@todo Be cosistent in use of ticks or clocks
    return 60 * g_nSampleRate / (dTempo *  g_dTicksPerBeat);
}

// Convert tempo to frames per clock
double getFramesPerClock(double dTempo)
{
    return getFramesPerTick(dTempo) * g_dTicksPerClock;
}

// Update bars, beats, ticks for given position in frames
void updateBBT(jack_position_t* position)
{
    //!@todo There is a rounding error where each beat is calculated to the nearest 256 frames (possibly period size) then corrected every 7 or 8 beats (at 44100fps, 256 period, 120BPM)
    // This may be because this is only called once every period so need to consider partial progress through clock
    // This might be fixed with recent code changes
    
    //!@todo Populate bbt_offset (experimental so not urgent but could be useful)
    double dFrames = 0;
    double dFramesPerTick = getFramesPerTick(g_dTempo); //!@todo Need to use default tempo from start of song but current tempo now!!!
    static double dDebugFramesPerTick = 0;
    uint32_t nBar = 0;
    uint32_t nBeat = 0;
    uint32_t nTick = 0;
    uint8_t nBeatsPerBar = DEFAULT_TIMESIG >> 8;
    uint8_t nBeatsType = DEFAULT_TIMESIG & 0x00FF;
    uint32_t nTicksPerBar = g_dTicksPerBeat * nBeatsPerBar;
    bool bDone = false;
    double dFramesInSection;
    uint32_t nTicksInSection;
    uint32_t nTicksFromStart = 0;

    // Let's just get this working without a tempo map until we implement song
    position->tick = position->frame % uint32_t(dFramesPerTick);
    position->beat = (uint32_t(position->frame / dFramesPerTick) % uint32_t(g_dTicksPerBeat)) + 1;
    position->bar = (uint32_t(position->frame / dFramesPerTick / g_dTicksPerBeat) % nBeatsPerBar) + 1;
    position->beats_per_bar = g_fBeatsPerBar;
    position->beats_per_minute = g_dTempo;
    position->beat_type = g_fBeatType;
    position->ticks_per_beat = g_dTicksPerBeat;
    position->bar_start_tick = 0; //!@todo Need to calculate this
    //g_pNextTimebaseEvent = g_pTimebase->getPreviousTimebaseEvent(position->bar, (position->beat - 1) * position->ticks_per_beat + position->tick  , TIMEBASE_TYPE_ANY);


    // Iterate through events, calculating quantity of frames between each event
    /*
    if(g_pTimebase)
    {
        for(size_t nIndex = 0; nIndex < g_pTimebase->getEventQuant(); ++nIndex)
        {
            // Get next event
            TimebaseEvent* pEvent = g_pTimebase->getEvent(nIndex);
            // Calculate quantity of ticks between events and frames between events
            nTicksInSection = (pEvent->bar * nTicksPerBar + pEvent->clock * g_dFramesPerClock - nTicksFromStart);
            dFramesInSection = nTicksInSection * dFramesPerTick;
            // Break if next event is beyond requested position
            if(dFrames + dFramesInSection > position->frame)
                break;
            // Update frame counter, bar and tick from which to count last section
            dFrames += dFramesInSection;
            nBar = pEvent->bar;
            nTick = pEvent->clock * g_dTicksPerClock;
            nTicksFromStart += nTicksInSection;
            // Update tempo and time signature from event
            if(pEvent->type == TIMEBASE_TYPE_TEMPO)
                dFramesPerTick = getFramesPerTick(pEvent->value);
            else if(pEvent->type == TIMEBASE_TYPE_TIMESIG)
            {
                nBeatsPerBar = pEvent->value >> 8;
                nBeatsType = pEvent->value & 0x00FF;
                nTicksPerBar = g_dTicksPerBeat * nBeatsPerBar;
            }
        }
    }
    */
    // Calculate BBT from last section
    dFramesInSection = position->frame - dFrames;
    nTicksInSection = dFramesInSection / dFramesPerTick;
    uint32_t nBarsInSection = nTicksInSection / nTicksPerBar;
    position->bar = nBar + nBarsInSection + 1;
    uint32_t nTicksInLastBar = nTicksInSection % nTicksPerBar;
    position->beat = nTicksInLastBar / g_dTicksPerBeat + 1;
    position->tick = nTicksInLastBar % position->beat;
    nTicksFromStart += nTicksInSection;
    position->bar_start_tick = nTicksFromStart - nTicksInLastBar;
    g_nClock = position->tick % (uint32_t)g_dTicksPerClock;
    g_nFramesToNextClock = 0; //!@todo Quantisation error?
    //g_dTempo = g_pTimebase->getTempo(g_nBar, (g_nBeat * g_dTicksPerBeat + g_nTick) / g_dTicksPerClock);
    //g_fBeatsPerBar = uint16_t(g_pTimebase->getTimeSig(g_nBar, (g_nBeat * g_dTicksPerBeat + g_nTick) / g_dTicksPerClock)) >> 8;
}

/*  Handle timebase callback - update timebase elements (BBT) from transport position
    nState: Current jack transport state
    nFramesInPeriod: Quantity of frames in current period
    pPosition: Pointer to position structure for the next cycle
    bUpdate: True (non-zero) to request position be updated to position defined in pPosition (also true on first callback)
    pArgs: Pointer to argument supplied by jack_set_timebase_callback (not used here)
    
    [Info]
    If bUpdate is false then calculate BBT from pPosition->frame: quantity of frames from start of song.
    If bUpdate is true then calculate pPostion-frame from BBT info

    [Process]
    Calculate bars, beats, ticks at pPosition->frame from start of song or calculate frame from BBT:
    Iterate through timebase events spliting song into sections delimited by timebase events: time signature / tempo changes, calculating BBT for each section up to current position.
    Add events from sequences to schedule
*/
void onJackTimebase(jack_transport_state_t nState, jack_nframes_t nFramesInPeriod,
          jack_position_t *pPosition, int bUpdate, void* pArgs)
{
    jack_nframes_t nRemainingFrames = nFramesInPeriod; // Quantity of frames in this period left to process for clock generation    

    // Process timebase events
    while(g_pTimebase && g_pNextTimebaseEvent && (g_pNextTimebaseEvent->bar <= g_nBar)) // || g_pNextTimebaseEvent->bar == g_nBar && g_pNextTimebaseEvent->clock <= g_nClock))
    {
        if(g_pNextTimebaseEvent->type == TIMEBASE_TYPE_TEMPO)
        {
            g_dTempo = g_pNextTimebaseEvent->value;
            g_dFramesPerClock = getFramesPerClock(g_dTempo);
            pPosition->beats_per_minute = g_dTempo;
            g_bTimebaseChanged = true;
            DPRINTF("Tempo change to %0.0fbpm frames/clk: %f\n", g_dTempo, g_dFramesPerClock);
        }
        else if(g_pNextTimebaseEvent->type == TIMEBASE_TYPE_TIMESIG)
        {
            g_fBeatsPerBar = g_pNextTimebaseEvent->value >> 8;
            g_fBeatType = g_pNextTimebaseEvent->value & 0x0F;
            pPosition->beats_per_bar = g_fBeatsPerBar;
            g_bTimebaseChanged = true;
            DPRINTF("Time signature change to %0.0f/%0.0f\n", g_fBeatsPerBar, g_fBeatType);
        }
        g_pNextTimebaseEvent = g_pTimebase->getNextTimebaseEvent(g_pNextTimebaseEvent);
    }

    // Calculate BBT at start of next period
    if(bUpdate || g_bTimebaseChanged)
    {
        if(g_pTimebase)
        {
            g_dTempo = g_pTimebase->getTempo(g_nBar, (g_nBeat * g_dTicksPerBeat + g_nTick));
            g_fBeatsPerBar = g_pTimebase->getTimeSig(g_nBar, (g_nBeat * g_dTicksPerBeat + g_nTick)) >> 8;
        }
        // Update position based on parameters passed
        if(pPosition->valid & JackPositionBBT)
        {
            // Set position from BBT
            DPRINTF("bUpdate: %s, g_bTimebaseChanged: %s, Position valid flags: %u\n", bUpdate?"True":"False", g_bTimebaseChanged?"True":"False", pPosition->valid);
            DPRINTF("PreSet position from BBT Bar: %u Beat: %u Tick: %u Clock: %u\n", pPosition->bar, pPosition->beat, pPosition->tick, g_nClock);
            DPRINTF("Beats per bar: %f\n", pPosition->beats_per_bar);
            // Fix overruns
            pPosition->beat += pPosition->tick / (uint32_t)pPosition->ticks_per_beat;
            pPosition->tick %= (uint32_t)(pPosition->ticks_per_beat);
            pPosition->bar += (pPosition->beat - 1) / pPosition->beats_per_bar;
            pPosition->beat = ((pPosition->beat - 1) % (uint32_t)(pPosition->beats_per_bar)) + 1;
            pPosition->frame = transportGetLocation(pPosition->bar, pPosition->beat, pPosition->tick);
            pPosition->ticks_per_beat = g_dTicksPerBeat;
            pPosition->beats_per_minute = g_dTempo;
            g_nClock = pPosition->tick / g_dTicksPerClock;
            g_nBar = pPosition->bar;
            g_nBeat = pPosition->beat;
            g_nTick = pPosition->tick;
            DPRINTF("Set position from BBT Bar: %u Beat: %u Tick: %u Clock: %u\n", pPosition->bar, pPosition->beat, pPosition->tick, g_nClock);
        }
        else
        {
            updateBBT(pPosition);
            DPRINTF("Set position from frame %u\n", pPosition->frame);
        }
        g_nTransportStartFrame = jack_frame_time(g_pJackClient) + pPosition->frame;
        pPosition->valid = JackPositionBBT;
        g_bTimebaseChanged = false;
        DPRINTF("New position: Jack frame: %u Frame: %u Bar: %u Beat: %u Tick: %u Clock: %u\n", g_nTransportStartFrame, pPosition->frame, pPosition->bar, pPosition->beat, pPosition->tick, g_nClock);
        //!@todo Check impact of timebase discontinuity
    }
    else
    {
        //DPRINTF("Update position with values from previous period Jack frame: %u Frame: %u Bar: %u Beat: %u Tick: %u Clock: %u\n", g_nTransportStartFrame, pPosition->frame, pPosition->bar, pPosition->beat, pPosition->tick, g_nClock);
        // Set BBT values calculated during previous period
        pPosition->bar = g_nBar;
        pPosition->beat = g_nBeat;
        pPosition->tick = g_nTick % (uint32_t)g_dTicksPerBeat;
        pPosition->bar_start_tick = g_dBarStartTick;
        pPosition->beats_per_bar = g_fBeatsPerBar;
        pPosition->beat_type = g_fBeatType;
        pPosition->ticks_per_beat = g_dTicksPerBeat;
        pPosition->beats_per_minute = g_dTempo;
        // Loop frame if not playing song
        if(!g_nBeat && isSongPlaying())
            pPosition->frame = transportGetLocation(pPosition->bar, pPosition->beat, pPosition->tick); //!@todo Does this work? (yes). Are there any discontinuity or impact on other clients? Can it be optimsed?
    }
    // Now iterate through clocks in next period, adding any events and handling any timebase changes
    if(transportGetPlayStatus() == JackTransportRolling)
    {
        //Returning here stops seg fault
        bool bSync = false;
        while(g_nFramesToNextClock < nRemainingFrames)
        {
            //!@todo I think segfault may be triggered within this while loop
            bSync = false;
            //!@todo Have added a period to clock position but it should already be offset as pPosition->frame refers to next cycle
            jack_nframes_t nClockPos = g_nFramesToNextClock + pPosition->frame + g_nTransportStartFrame + nFramesInPeriod; // Absolute position of clock within next period
            if(!g_nClock)
            {
                // Clock zero so on beat
                bSync = (g_nBeat == 1);
                g_nTick = g_dTicksPerBeat * (g_nBeat - 1);
                if(g_nSongStatus == PLAYING && ++g_nSongPosition > g_nSongLength)
                    g_nSongStatus = STOPPED;
                if(bSync && g_nSongStatus == STARTING)
                        g_nSongStatus = PLAYING; // Start song at start of bar
            }
            // Schedule events in next period
            g_bPlaying = PatternManager::getPatternManager()->clock(nClockPos, &g_mSchedule, bSync, g_dFramesPerClock); // Pass clock time and schedule to pattern manager so it can populate with events. Pass sync pulse so that it can syncronise its sequences, e.g. start zynpad sequences
            nRemainingFrames -= g_nFramesToNextClock;
            g_nFramesToNextClock = g_dFramesPerClock;
            if(++g_nClock > 23)
            {
                g_nClock = 0;
                if(++g_nBeat > g_fBeatsPerBar)
                {
                    g_nBeat = 1;
                    if(g_nSongStatus == PLAYING)
                        ++g_nBar;
                }
                DPRINTF("Beat %u of %f\n", g_nBeat, g_fBeatsPerBar);
            }
        }
        g_nFramesToNextClock -= nRemainingFrames;
        g_nTick = g_dTicksPerBeat - nRemainingFrames / getFramesPerTick(g_dTempo);
        if(bSync && !g_bPlaying)
        {
            //!@todo We stop at end of bar to encourage previous block of code to run but we may prefer to stop more promptly
            DPRINTF("Stopping transport because no sequences playing clock: %u beat: %u tick: %u\n", g_nClock, g_nBeat, g_nTick);
            transportStop();
            transportLocate(0);
        }
    }
}

/*  Process jack cycle - must complete within single jack period
    nFrames: Quantity of frames in this period
    pArgs: Parameters passed to function by main thread (not used here)

    [For info]
    jack_last_frame_time is the quantity of samples since JACK started
    jack_midi_event_write sends MIDI message at sample time offset

    [Process]
    Process incoming MIDI events
    Iterate through events scheduled to trigger within this process period
    For each event, add MIDI events to the output buffer at appropriate sample offset
    Remove events from schedule
*/
int onJackProcess(jack_nframes_t nFrames, void *pArgs)
{
    static jack_position_t transportPosition; // JACK transport position structure populated each cycle and checked for transport progress
    static bool bSync = false; // Track if we have sent a sync pulse for this bar
    static uint8_t nClock = 24; // Clock pulse count 0..23
    static uint32_t nTicksPerPulse;
    static double dTicksPerFrame;
    static double dBeatsPerMinute; // Store so that we can check for change and do less maths
    static double dBeatsPerBar; // Store so that we can check for change and do less maths
    static jack_nframes_t nFramerate; // Store so that we can check for change and do less maths
    static uint32_t nFramesPerPulse;
    
    // Get output buffer that will be processed in this process cycle
    void* pOutputBuffer = jack_port_get_buffer(g_pOutputPort, nFrames);
    unsigned char* pBuffer;
    jack_midi_clear_buffer(pOutputBuffer);
    jack_nframes_t nNow = jack_last_frame_time(g_pJackClient);

    // Process MIDI input
    void* pInputBuffer = jack_port_get_buffer(g_pInputPort, nFrames);
    jack_midi_event_t midiEvent;
    jack_nframes_t nCount = jack_midi_get_event_count(pInputBuffer);
    for(jack_nframes_t i = 0; i < nCount; i++)
    {
        jack_midi_event_get(&midiEvent, pInputBuffer, i);
        switch(midiEvent.buffer[0])
        {
            case MIDI_STOP:
                DPRINTF("StepJackClient MIDI STOP\n");
                //!@todo Send note off messages
                pauseSong();
                break;
            case MIDI_START:
                DPRINTF("StepJackClient MIDI START\n");
                stopSong();
                startSong();
                break;
            case MIDI_CONTINUE:
                DPRINTF("StepJackClient MIDI CONTINUE\n");
                startSong();
                break;
            case MIDI_CLOCK:
                DPRINTF("StepJackClient MIDI CLOCK\n");
                // Ignore MIDI clock - let Jack timebase master handle it
                break;
            case MIDI_POSITION:
            {
                //!@todo Should we let Jack timebae master manage MIDI position changes?
                uint32_t nPos = (midiEvent.buffer[1] + (midiEvent.buffer[2] << 7)) * 6;
                DPRINTF("StepJackClient POSITION %d (clocks)\n", nPos);
                setSongPosition(nPos);
                break;
            }
            case MIDI_SONG:
                DPRINTF("StepJackClient Select song %d\n", midiEvent.buffer[1]);
                selectSong(midiEvent.buffer[1] + 1);
                break;
            default:
                break;
        }
        // Handle MIDI Note On events to trigger sequences
        if((midiEvent.buffer[0] == (MIDI_NOTE_ON | PatternManager::getPatternManager()->getTriggerChannel())) && midiEvent.buffer[2])
        {
            if(getPlayState(PatternManager::getPatternManager()->trigger(midiEvent.buffer[1])) != STOPPED)
                transportStart();
        }
        // Handle MIDI Note On events for programming patterns from MIDI input
        if(PatternManager::getPatternManager()->getCurrentSong() == 0 && g_nInputChannel < 16 && (midiEvent.buffer[0] == (MIDI_NOTE_ON | g_nInputChannel)) && midiEvent.buffer[2])
        {
            if(g_pPattern)
            {
                Sequence* pSeq = PatternManager::getPatternManager()->getSequence(1);
                uint32_t nStep = pSeq->getStep();
                if(getNoteVelocity(nStep, midiEvent.buffer[1]))
                    g_pPattern->removeNote(nStep, midiEvent.buffer[1]);
                else
                    g_pPattern->addNote(nStep, midiEvent.buffer[1], midiEvent.buffer[2], 1);
                if(transportGetPlayStatus() != JackTransportRolling)
                {
                    if(++nStep >= g_pPattern->getSteps())
                        nStep = 0;
                    pSeq->setStep(nStep);
                }
            }
        }
    }

    // Send MIDI output aligned with first sample of frame resulting in similar latency to audio
    //!@todo Interpolate events across frame, e.g. CC variations
    while(g_bMutex)
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    g_bMutex = true;
    if(g_mSchedule.size())
    {
        auto it = g_mSchedule.begin();
        jack_nframes_t nTime, nNextTime = 0;
        while(it != g_mSchedule.end())
        {
            if(it->first >= nNow + nFrames)
                break; // Event scheduled beyond this buffer
            if(it->first < nNow)
            {
                nTime = nNextTime; // This event is in the past so send as soon as possible
                // If lots of events are added in past then they may be sent out of order because frame boundary may be hit and existing events will be left in their previous position, e.g. at time 57 then up to 56 events could be inserted before this. Low risk and only for immediate events so unlikely to have significant impact.
                DPRINTF("Sending event from past (Scheduled:%u Now:%u Diff:%d samples)\n", it->first, nNow, nNow - it->first);
            }
            else
                nTime = it->first - nNow; // Schedule event at scheduled time offset
            if(nTime < nNextTime)
                nTime = nNextTime; // Ensure we send events in order - this may bump events slightly latter than scheduled (by individual frames (samples) so not much)
            if(nTime >= nFrames)
            {
                g_bMutex = false;
                return 0; // Must have bumped beyond end of this frame time so must wait until next frame
            }
            nNextTime = nTime + 1;
            // Get a pointer to the next 3 available bytes in the output buffer
            //!@todo Should we use correct buffer size based on MIDI message size, e.g. 1 byte for realtime messages?
            pBuffer = jack_midi_event_reserve(pOutputBuffer, nTime, 3);
            if(pBuffer == NULL)
                break; // Exceeded buffer size (or other issue)
            if(it->second)
            {
                pBuffer[0] = it->second->command;
                pBuffer[1] = it->second->value1;
                pBuffer[2] = it->second->value2;
                delete it->second;
                it->second = NULL;
            }
            ++it;
            DPRINTF("Sending MIDI event %d,%d,%d at %u\n", pBuffer[0],pBuffer[1],pBuffer[2], nNow + nTime);
        }
        g_mSchedule.erase(g_mSchedule.begin(), it);
    }
    g_bMutex = false;
    return 0;
}

int onJackSampleRateChange(jack_nframes_t nFrames, void *pArgs)
{
    DPRINTF("zynseq: Jack sample rate: %d\n", nFrames);
    g_nSampleRate = nFrames;
    g_dFramesPerClock = getFramesPerClock(g_dTempo);
    return 0;
}

int onJackXrun(void *pArgs)
{
    DPRINTF("zynseq detected XRUN\n");
    return 0;
}

void end()
{
    DPRINTF("zynseq exit\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    for(auto it : g_mSchedule)
    {
        delete it.second;
    }
}

// ** Library management functions **

bool init(bool bTimebaseMaster)
{
    // Register with Jack server
    char *sServerName = NULL;
    jack_status_t nStatus;
    jack_options_t nOptions = JackNoStartServer;
    
    if(g_pJackClient)
        return false; // Already initialised

    if((g_pJackClient = jack_client_open("zynthstep", nOptions, &nStatus, sServerName)) == 0)
    {
        fprintf(stderr, "libzynseq failed to start jack client: %d\n", nStatus);
        return false;
    }
    
    // Create input port
    if(!(g_pInputPort = jack_port_register(g_pJackClient, "input", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0)))
    {
        fprintf(stderr, "libzynseq cannot register input port\n");
        return false;
    }

    // Create output port
    if(!(g_pOutputPort = jack_port_register(g_pJackClient, "output", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0)))
    {
        fprintf(stderr, "libzynseq cannot register output port\n");
        return false;
    }

    // Register JACK callbacks
    jack_set_process_callback(g_pJackClient, onJackProcess, 0);
    jack_set_sample_rate_callback(g_pJackClient, onJackSampleRateChange, 0);
    jack_set_xrun_callback(g_pJackClient, onJackXrun, 0); //!@todo Remove xrun handler (just for debug)

    if(jack_activate(g_pJackClient)) {
        fprintf(stderr, "libzynseq cannot activate client\n");
        return false;
    }

    if(bTimebaseMaster)
        if(transportRequestTimebase())
            DPRINTF("Registered as timebase master\n");
        else
            DPRINTF("Failed to register as timebase master\n");

    // Register the cleanup function to be called when program exits
    atexit(end);
    
    selectSong(1);
    
    transportStop();
    transportLocate(0);
    return true;
}

bool load(char* filename)
{
    return PatternManager::getPatternManager()->load(filename);
}

void save(char* filename)
{
    PatternManager::getPatternManager()->save(filename);
}


// ** Direct MIDI interface **

// Schedule a MIDI message to be sent in next JACK process cycle
void sendMidiMsg(MIDI_MESSAGE* pMsg)
{
    //!@todo Should we schedule MIDI messages to be sent at offset within period?
    // Find first available time slot
    uint32_t time = 0;
    while(g_bMutex)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    g_bMutex = true;
    while(g_mSchedule.find(time) != g_mSchedule.end())
        ++time;
    g_mSchedule[time] = pMsg;
    g_bMutex = false;
}

// Schedule a note off event after 'duration' ms
void noteOffTimer(uint8_t note, uint8_t channel, uint32_t duration)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
    MIDI_MESSAGE* pMsg = new MIDI_MESSAGE;
    pMsg->command = MIDI_NOTE_ON | (channel & 0x0F);
    pMsg->value1 = note;
    pMsg->value2 = 0;
    sendMidiMsg(pMsg);
}

void playNote(uint8_t note, uint8_t velocity, uint8_t channel, uint32_t duration)
{
    if(note>127 || velocity > 127 || channel > 15 || duration >60000)
        return;
    MIDI_MESSAGE* pMsg = new MIDI_MESSAGE;
    pMsg->command = MIDI_NOTE_ON | channel;
    pMsg->value1 = note;
    pMsg->value2 = velocity;
    sendMidiMsg(pMsg);
    if(duration)
    {
        std::thread noteOffThread(noteOffTimer, note, channel, duration);
        noteOffThread.detach();
    }
}

//!@todo Do we still need functions to send MIDI transport control (start, stop, continuew, songpos, song select, clock)?

void sendMidiStart()
{
    MIDI_MESSAGE* pMsg = new MIDI_MESSAGE;
    pMsg->command = MIDI_START;
    sendMidiMsg(pMsg);
    DPRINTF("Sending MIDI Start... does it get recieved back???\n");
}

void sendMidiStop()
{
    MIDI_MESSAGE* pMsg = new MIDI_MESSAGE;
    pMsg->command = MIDI_STOP;
    sendMidiMsg(pMsg);
}

void sendMidiContinue()
{
    MIDI_MESSAGE* pMsg = new MIDI_MESSAGE;
    pMsg->command = MIDI_CONTINUE;
    sendMidiMsg(pMsg);
}

void sendMidiSongPos(uint16_t pos)
{
    MIDI_MESSAGE* pMsg = new MIDI_MESSAGE;
    pMsg->command = MIDI_POSITION;
    pMsg->value1 = pos & 0x7F;
    pMsg->value2 = (pos >> 7) & 0x7F;
    sendMidiMsg(pMsg);
}

void sendMidiSong(uint32_t pos)
{
    if(pos > 127)
        return;
    MIDI_MESSAGE* pMsg = new MIDI_MESSAGE;
    pMsg->command = MIDI_SONG;
    pMsg->value1 = pos & 0x7F;
    sendMidiMsg(pMsg);
}

// Send a single MIDI clock
void sendMidiClock()
{
    MIDI_MESSAGE* pMsg = new MIDI_MESSAGE;
    pMsg->command = MIDI_CLOCK;
    sendMidiMsg(pMsg);
}

uint8_t getTriggerChannel()
{
    return PatternManager::getPatternManager()->getTriggerChannel();
}

void setTriggerChannel(uint8_t channel)
{
    if(channel > 15)
        return;
    PatternManager::getPatternManager()->setTriggerChannel(channel);
}

uint8_t getTriggerNote(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getTriggerNote(sequence);
}

void setTriggerNote(uint32_t sequence, uint8_t note)
{
    PatternManager::getPatternManager()->setTriggerNote(sequence, note);
}

// ** Pattern management functions **

void selectPattern(uint32_t pattern)
{
    g_pPattern = PatternManager::getPatternManager()->getPattern(pattern);
    if(g_pPattern)
        g_nPattern = pattern;
    g_bModified = true;
}

uint32_t getPatternIndex()
{
    return g_nPattern;
}

uint32_t getSteps()
{
    if(g_pPattern)
        return g_pPattern->getSteps();
    return 0;
}

uint32_t getPatternLength(uint32_t pattern)
{
    Pattern* pPattern = PatternManager::getPatternManager()->getPattern(pattern);
    if(pPattern)
        return pPattern->getLength();
    return 0;
}

uint32_t getBeatsInPattern()
{
    if(g_pPattern)
        return g_pPattern->getBeatsInPattern();
    return 0;
}

void setBeatsInPattern(uint32_t beats)
{
    if(!g_pPattern)
        return;
    g_pPattern->setBeatsInPattern(beats);
    PatternManager::getPatternManager()->updateAllSequenceLengths();
    g_bModified = true;
    //!@todo Update song length if relevant
}

uint32_t getClocksPerStep()
{
    if(g_pPattern)
        return g_pPattern->getClocksPerStep();
    return 6;
}

uint32_t getStepsPerBeat()
{
    if(g_pPattern)
        return g_pPattern->getStepsPerBeat();
    return 4;
}

void setStepsPerBeat(uint32_t steps)
{
    if(!g_pPattern)
        return;
    g_pPattern->setStepsPerBeat(steps);
    g_bModified = true;
}

uint8_t getBeatType()
{
    if(!g_pPattern)
        return 4;
    return g_pPattern->getBeatType();
}

void setBeatType(uint8_t beatType)
{
    if(!g_pPattern)
        return;
    g_pPattern->setBeatType(beatType);
    g_bModified = true;
}

bool addNote(uint32_t step, uint8_t note, uint8_t velocity, uint32_t duration)
{
    if(!g_pPattern)
        return false;
    g_bModified = true;
    return g_pPattern->addNote(step, note, velocity, duration);
}

void removeNote(uint32_t step, uint8_t note)
{
    if(!g_pPattern)
        return;
    g_bModified = true;
    g_pPattern->removeNote(step, note);
}

uint8_t getNoteVelocity(uint32_t step, uint8_t note)
{
    if(g_pPattern)
        return g_pPattern->getNoteVelocity(step, note);
    return 0;
}

void setNoteVelocity(uint32_t step, uint8_t note, uint8_t velocity)
{
    if(!g_pPattern)
        return;
    g_bModified = true;
    g_pPattern->setNoteVelocity(step, note, velocity);
}

uint32_t getNoteDuration(uint32_t step, uint8_t note)
{
    if(g_pPattern)
        return g_pPattern->getNoteDuration(step, note);
    return 0;
}

void transpose(int8_t value)
{
    if(!g_pPattern)
        return;
    g_bModified = true;
    g_pPattern->transpose(value);
}

void clear()
{
    if(!g_pPattern)
        return;
    g_bModified = true;
    g_pPattern->clear();
}

void copyPattern(uint32_t source, uint32_t destination)
{
    PatternManager::getPatternManager()->copyPattern(source, destination);
}

void setInputChannel(uint8_t channel)
{
    if(channel > 15)
        g_nInputChannel = 0xFF;
    g_nInputChannel = channel;
}

uint8_t getInputChannel()
{
    return g_nInputChannel;
}

void setScale(uint32_t scale)
{
    if(g_pPattern)
        g_pPattern->setScale(scale);
}

uint32_t getScale()
{
    if(g_pPattern)
        return g_pPattern->getScale();
    return 0;
}

void setTonic(uint8_t tonic)
{
    if(g_pPattern)
        g_pPattern->setTonic(tonic);
}

uint8_t getTonic()
{
    if(g_pPattern)
        return g_pPattern->getTonic();
    return 0;
}

bool isPatternModified()
{
    if(g_bModified)
    {
        g_bModified = false;
        return true;
    }
    return false;
}

// ** Sequence management functions **

uint32_t getStep(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getSequence(sequence)->getStep();
}

bool addPattern(uint32_t sequence, uint32_t position, uint32_t pattern, bool force)
{
    PatternManager* pPm = PatternManager::getPatternManager();
    bool bUpdated = pPm->getSequence(sequence)->addPattern(position, pPm->getPattern(pattern), force);
    g_nSongLength = PatternManager::getPatternManager()->updateSequenceLengths(pPm->getCurrentSong());
    return bUpdated;
}

void removePattern(uint32_t sequence, uint32_t position)
{
    PatternManager* pPm = PatternManager::getPatternManager();
    pPm->getSequence(sequence)->removePattern(position);
    g_nSongLength = PatternManager::getPatternManager()->updateSequenceLengths(pPm->getCurrentSong());
}

uint32_t getPattern(uint32_t sequence, uint32_t position)
{
    PatternManager* pPm = PatternManager::getPatternManager();
    Sequence* pSeq = pPm->getSequence(sequence);
    Pattern* pPattern = pSeq->getPattern(position);
    return pPm->getPatternIndex(pPattern);
}

void setChannel(uint32_t sequence, uint8_t channel)
{
    PatternManager::getPatternManager()->getSequence(sequence)->setChannel(channel);
}

uint8_t getChannel(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getSequence(sequence)->getChannel(); 
}

void setOutput(uint32_t sequence, uint8_t output)
{
    PatternManager::getPatternManager()->getSequence(sequence)->setOutput(output);
}

uint8_t getPlayMode(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getSequence(sequence)->getPlayMode();
}

void setPlayMode(uint32_t sequence, uint8_t mode)
{
    PatternManager::getPatternManager()->getSequence(sequence)->setPlayMode(mode);
 }

uint8_t getPlayState(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getSequence(sequence)->getPlayState();
}

void setPlayState(uint32_t sequence, uint8_t state)
{
    if(transportGetPlayStatus() != JackTransportRolling)
    {
        if(state == STARTING)
        {
//            state = PLAYING;
            PatternManager::getPatternManager()->setSequencePlayState(sequence, state);
            setTransportToStartOfBar();
            transportStart();
            return;
        }
        else if(state == STOPPING)
            state = STOPPED;
    }
    PatternManager::getPatternManager()->setSequencePlayState(sequence, state);
}

void togglePlayState(uint32_t sequence)
{
    uint8_t nState = PatternManager::getPatternManager()->getSequence(sequence)->getPlayState();
    nState = (nState == STOPPED || nState == STOPPING)?STARTING:STOPPING;
    setPlayState(sequence, nState);
}

void stop()
{
    stopSong();
    PatternManager::getPatternManager()->stop();
}

uint32_t getPlayPosition(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getSequence(sequence)->getPlayPosition();
}

void setPlayPosition(uint32_t sequence, uint32_t clock)
{
    PatternManager::getPatternManager()->getSequence(sequence)->setPlayPosition(clock);
}

uint32_t getSequenceLength(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getSequence(sequence)->getLength();
}

void clearSequence(uint32_t sequence)
{
    // This is only used by pattern editor
    PatternManager::getPatternManager()->getSequence(sequence)->clear();
    PatternManager::getPatternManager()->updateAllSequenceLengths();
    //!@todo Update song length if required
}


uint8_t getGroup(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getSequence(sequence)->getGroup();
}

void setGroup(uint32_t sequence, uint8_t group)
{
    PatternManager::getPatternManager()->getSequence(sequence)->setGroup(group);
}

uint8_t getTallyChannel(uint32_t sequence)
{
    return PatternManager::getPatternManager()->getSequence(sequence)->getTallyChannel();
}

void setTallyChannel(uint32_t sequence, uint8_t channel)
{
    PatternManager::getPatternManager()->getSequence(sequence)->setTallyChannel(channel);
}


// ** Song management functions **

uint32_t addTrack(uint32_t song)
{
    return PatternManager::getPatternManager()->addTrack(song);
}

void removeTrack(uint32_t song, uint32_t track)
{
    PatternManager::getPatternManager()->removeTrack(song, track);
    g_nSongLength = PatternManager::getPatternManager()->updateSequenceLengths(song);
}

void setTempo(uint32_t song, uint32_t tempo, uint16_t bar, uint16_t tick)
{
    PatternManager::getPatternManager()->getSong(song)->setTempo(tempo, bar, tick);
    if(song == getSong())
    {
        g_bTimebaseChanged = true;
        if(g_pTimebase)
            g_pNextTimebaseEvent = g_pTimebase->getFirstTimebaseEvent(); //!@todo Overkill parsing whole timebase map
    }
}

uint32_t getTempo(uint32_t song, uint16_t bar, uint16_t tick)
{
    return PatternManager::getPatternManager()->getSong(song)->getTempo(bar, tick);
}

void setTimeSig(uint32_t song, uint8_t beats, uint8_t type, uint16_t bar)
{
    if(bar < 1)
        bar = 1;
    PatternManager::getPatternManager()->getSong(song)->setTimeSig((beats << 8) | type, bar);
    if(song == getSong())
    {
        g_bTimebaseChanged = true;
        if(g_pTimebase)
            g_pNextTimebaseEvent = g_pTimebase->getFirstTimebaseEvent(); //!@todo Overkill parsing whole timebase map
    }
}

uint16_t getTimeSig(uint32_t song, uint16_t bar)
{
    return PatternManager::getPatternManager()->getSong(song)->getTimeSig(bar);
}

uint8_t getBeatsPerBar(uint32_t song, uint16_t bar)
{
    return getTimeSig(song, bar) >> 8;
}

uint8_t getBeatType(uint32_t song, uint16_t bar)
{
    return getTimeSig(song, bar) & 0xFF;
}

uint32_t getTracks(uint32_t song)
{
    return PatternManager::getPatternManager()->getSong(song)->getTracks();
}

uint32_t getSequence(uint32_t song, uint32_t track)
{
    return PatternManager::getPatternManager()->getSong(song)->getSequence(track);
}

void clearSong(uint32_t song)
{
    PatternManager::getPatternManager()->clearSong(song);
    g_nSongLength = 0;
}

void copySong(uint32_t source, uint32_t destination)
{
    PatternManager::getPatternManager()->copySong(source, destination);
}

void startSong(bool bFast)
{
    PatternManager::getPatternManager()->startSong(bFast);
    g_nSongStatus = bFast?PLAYING:STARTING;
}

void pauseSong()
{
    g_nSongStatus = STOPPED;
    PatternManager::getPatternManager()->stopSong();
}

void stopSong()
{
    g_nSongStatus = STOPPED;
    PatternManager::getPatternManager()->stopSong();
//  PatternManager::getPatternManager()->getSequence(0)->setPlayState(STOPPED);
    setSongPosition(0);
}

void toggleSong()
{
    if(g_nSongStatus == STOPPED)
        startSong();
    else
        pauseSong();
}

bool isSongPlaying()
{ 
    return g_nSongStatus == PLAYING;
}

void setSongPosition(uint32_t pos)
{
    PatternManager::getPatternManager()->setSongPosition(pos);
    g_nSongPosition = pos;
    //!@todo Update g_pNextTimebaseEvent

}

void setTransportToStartOfBar()
{
    jack_position_t position;
    jack_transport_query(g_pJackClient, &position);
    position.beat = 1;
    position.tick = 0;
//    position.valid = JackPositionBBT;
    jack_transport_reposition(g_pJackClient, &position);
//    g_pNextTimebaseEvent = g_pTimebase->getPreviousTimebaseEvent(position.bar, 1, TIMEBASE_TYPE_ANY); //!@todo Might miss event if 2 at start of bar
}

uint32_t getSongPosition()
{
    return g_nSongPosition;
}

uint32_t getSong()
{
    return PatternManager::getPatternManager()->getCurrentSong();
}

void selectSong(uint32_t song)
{
    DPRINTF("Selecting song %d\n", song);
//  stopSong();
    PatternManager::getPatternManager()->setCurrentSong(song);
    g_nSongLength = PatternManager::getPatternManager()->updateSequenceLengths(song);
    g_pTimebase = PatternManager::getPatternManager()->getSong(song)->getTimebase();
    if(g_pTimebase)
        g_pNextTimebaseEvent = g_pTimebase->getFirstTimebaseEvent();
}

void solo(uint32_t song, uint32_t track, int solo)
{
    Song* pSong = PatternManager::getPatternManager()->getSong(song);
    uint32_t nSequence;
    for(uint32_t i = 0; i < getTracks(song); ++i)
    {
        nSequence = pSong->getSequence(i);
        PatternManager::getPatternManager()->getSequence(nSequence)->solo(false);
        setPlayState(nSequence, STOPPED);
    }
    nSequence = pSong->getSequence(track);
    PatternManager::getPatternManager()->getSequence(nSequence)->solo(solo);
    if(solo && g_nSongStatus == PLAYING)
        setPlayState(nSequence, PLAYING);
}

void transportLocate(uint32_t frame)
{
    jack_transport_locate(g_pJackClient, frame);
}

/*  Calculate the song position in frames from BBT
*/
jack_nframes_t transportGetLocation(uint32_t bar, uint32_t beat, uint32_t tick)
{
    // Convert one-based bars and beats to zero-based 
    if(bar > 0)
        --bar;
    if(beat > 0)
        --beat;    
    uint32_t nTicksToPrev = 0;
    uint32_t nTicksToEvent = 0;
    uint32_t nTicksPerBar = g_dTicksPerBeat * (DEFAULT_TIMESIG >> 8);
    double dFramesPerTick = getFramesPerTick(DEFAULT_TEMPO);
    double dFrames = 0; // Frames to position
    /*
    if(g_pTimebase)
    {
        for(size_t nIndex = 0; nIndex < g_pTimebase->getEventQuant(); ++nIndex)
        {
            TimebaseEvent* pEvent = g_pTimebase->getEvent(nIndex);
            if(pEvent->bar > bar || pEvent->bar == bar && pEvent->clock > (g_dTicksPerBeat * beat + tick) / g_dTicksPerBeat / 24)
                break; // Ignore events later than new position
            nTicksToEvent = pEvent->bar * nTicksPerBar + pEvent->clock * g_dTicksPerBeat / 24;
            uint32_t nTicksInBlock = nTicksToEvent - nTicksToPrev;
            dFrames += dFramesPerTick * nTicksInBlock;
            nTicksToPrev = nTicksToEvent;
            if(pEvent->type == TIMEBASE_TYPE_TEMPO)
                dFramesPerTick = getFramesPerTick(pEvent->value);
            else if(pEvent->type == TIMEBASE_TYPE_TIMESIG)
                nTicksPerBar = g_dTicksPerBeat * (pEvent->value >> 8);
        }
    }
    */
    dFrames += dFramesPerTick * (bar * nTicksPerBar + beat * g_dTicksPerBeat + tick - nTicksToPrev);
    return dFrames;
}

bool transportRequestTimebase()
{
    if(jack_set_timebase_callback(g_pJackClient, 0, onJackTimebase, NULL))
        return false;
    return true;
}

void transportReleaseTimebase()
{
    jack_release_timebase(g_pJackClient);
}

void transportStart()
{
    jack_transport_start(g_pJackClient);
}

void transportStop()
{
    jack_transport_stop(g_pJackClient);
}

void transportToggle()
{
    if(transportGetPlayStatus() == JackTransportRolling)
        transportStop();
    else
        transportStart();
}

uint8_t transportGetPlayStatus()
{
    jack_position_t position; // Not used but required to query transport
    jack_transport_state_t nState;
    return jack_transport_query(g_pJackClient, &position);
}

void transportSetTempo(uint32_t tempo)
{
    g_dTempo = tempo;
    g_bTimebaseChanged = true; //!@todo Need to validate that this is required and behaves as expected
}

uint32_t transportGetTempo()
{
    return g_dTempo;
}

void transportSetSyncTimeout(uint32_t timeout)
{
    jack_set_sync_timeout(g_pJackClient, timeout);
}

