#include "pattern.h"
#include <cmath>

/**    Pattern class methods implementation **/

Pattern::Pattern(uint32_t beats, uint32_t stepsPerBeat) :
    m_nBeats(beats),
    m_nStepsPerBeat(stepsPerBeat)
{
    setStepsPerBeat(stepsPerBeat);
}

Pattern::~Pattern()
{
}

StepEvent* Pattern::addEvent(uint32_t position, uint8_t command, uint8_t value1, uint8_t value2, float duration)
{
    //Delete overlapping events
    for(auto it = m_vEvents.begin(); it!=m_vEvents.end(); ++it)
    {
        uint32_t nEventStart = position;
        float fEventEnd = nEventStart + duration;
        uint32_t nCheckStart = (*it).getPosition();
        float fCheckEnd = nCheckStart + (*it).getDuration();
        bool bOverlap = (nCheckStart >= nEventStart && nCheckStart < fEventEnd) || (fCheckEnd > nEventStart && fCheckEnd <= fEventEnd);
        if(bOverlap && (*it).getCommand() == command && (*it).getValue1start() == value1)
        {
            it = m_vEvents.erase(it) - 1;
            if(it == m_vEvents.end())
                break;
        }
    }
    uint32_t nTime = position % (m_nBeats * m_nStepsPerBeat);
    auto it = m_vEvents.begin();
    for(; it != m_vEvents.end(); ++it)
    {
        if((*it).getPosition() > position)
            break;
    }
    auto itInserted = m_vEvents.insert(it, StepEvent(position, command, value1, value2, duration));
    return &(*itInserted);
}

StepEvent* Pattern::addEvent(StepEvent* pEvent)
{
    StepEvent* pNewEvent = addEvent(pEvent->getPosition(), pEvent->getCommand(), pEvent->getValue1start(), pEvent->getValue2start(), pEvent->getDuration());
    pNewEvent->setValue1end(pEvent->getValue1end());
    pNewEvent->setValue2end(pEvent->getValue2end());
    return pNewEvent;
}

void Pattern::deleteEvent(uint32_t position, uint8_t command, uint8_t value1)
{
    for(auto it = m_vEvents.begin(); it != m_vEvents.end(); ++it)
    {
        if(it->getPosition() == position && it->getCommand() == command && it->getValue1start() == value1)
        {
            m_vEvents.erase(it);
            return;
        }
    }
}

bool Pattern::addNote(uint32_t step, uint8_t note, uint8_t velocity, float duration)
{
    //!@todo Should we limit note length to size of pattern?
    if(step >= (m_nBeats * m_nStepsPerBeat) || note > 127 || velocity > 127) // || duration > (m_nBeats * m_nStepsPerBeat))
        return false;
    addEvent(step, MIDI_NOTE_ON, note, velocity, duration);
    return true;
}

void Pattern::removeNote(uint32_t step, uint8_t note)
{
    deleteEvent(step, MIDI_NOTE_ON, note);
}

int32_t Pattern::getNoteStart(uint32_t step, uint8_t note)
{
    for(StepEvent ev : m_vEvents)
        if(ev.getPosition() <= step && int(std::ceil(ev.getPosition() + ev.getDuration())) > step && ev.getCommand() == MIDI_NOTE_ON && ev.getValue1start() == note)
            return ev.getPosition();
    return -1;
}

uint8_t Pattern::getNoteVelocity(uint32_t step, uint8_t note)
{
    for(StepEvent ev : m_vEvents)
        if(ev.getPosition() == step && ev.getCommand() == MIDI_NOTE_ON && ev.getValue1start() == note)
        return ev.getValue2start();
    return 0;
}

void Pattern::setNoteVelocity(uint32_t step, uint8_t note, uint8_t velocity)
{
    if(velocity > 127)
        return;
    for(StepEvent ev : m_vEvents)
        if(ev.getPosition() == step && ev.getCommand() == MIDI_NOTE_ON && ev.getValue1start() == note)
        {
            ev.setValue2start(velocity);
            return;
        }
}

float Pattern::getNoteDuration(uint32_t step, uint8_t note)
{
    if(step >= (m_nBeats * m_nStepsPerBeat))
        return 0;
    for(StepEvent ev : m_vEvents)
    {
        if(ev.getPosition() != step || ev.getCommand() != MIDI_NOTE_ON || ev.getValue1start() != note)
            continue;
        return ev.getDuration();
    }
    return 0.0;
}

bool Pattern::addProgramChange(uint32_t step, uint8_t program)
{
    if(step >= (m_nBeats * m_nStepsPerBeat) || program > 127)
        return false;
    removeProgramChange(step); // Only one PC per step
    addEvent(step, MIDI_PROGRAM, program);
    return true;
}

bool Pattern::removeProgramChange(uint32_t step)
{
    if(step >= (m_nBeats * m_nStepsPerBeat))
        return false;
    uint8_t program = getProgramChange(step);
    if(program == 0xFF)
        return false;
    deleteEvent(step, MIDI_PROGRAM, program);
    return true;
}

uint8_t Pattern::getProgramChange(uint32_t step)
{
    if(step >= (m_nBeats * m_nStepsPerBeat))
        return 0xFF;
    for(StepEvent ev : m_vEvents)
    {
        if(ev.getPosition() != step || ev.getCommand() != MIDI_PROGRAM)
            continue;
        return ev.getValue1start();
    }
    return 0xFF;
}

void Pattern::addControl(uint32_t step, uint8_t control, uint8_t valueStart, uint8_t valueEnd, float duration)
{
    float fDuration = duration;
    if(step > (m_nBeats * m_nStepsPerBeat) || control > 127 || valueStart > 127|| valueEnd > 127 || fDuration > (m_nBeats * m_nStepsPerBeat))
        return;
    StepEvent* pControl = new StepEvent(step, control, valueStart, fDuration);
    pControl->setValue2end(valueEnd);
    StepEvent* pEvent = addEvent(step, MIDI_CONTROL, control, valueStart, fDuration);
    pEvent->setValue2end(valueEnd);
}

void Pattern::removeControl(uint32_t step, uint8_t control)
{
    deleteEvent(step, MIDI_CONTROL, control);
}

float Pattern::getControlDuration(uint32_t step, uint8_t control)
{
    //!@todo Implement getControlDuration
    return 0.0;
}

uint32_t Pattern::getSteps()
{
    return (m_nBeats * m_nStepsPerBeat);
}


uint32_t Pattern::getLength()
{
    return m_nBeats * 24;
}

uint32_t Pattern::getClocksPerStep()
{
    if(m_nStepsPerBeat > 24 || m_nStepsPerBeat == 0)
        return 1;
    return 24 / m_nStepsPerBeat;
}

bool Pattern::setStepsPerBeat(uint32_t value)
{
    float fScale = 1.0;
    if(m_nStepsPerBeat == 0 || m_nStepsPerBeat > 24)
        m_nStepsPerBeat = 4;
    else
        float fScale = float(value) / m_nStepsPerBeat;

    switch(value)
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 6:
        case 8:
        case 12:
        case 24:
            m_nStepsPerBeat = value;
            break;
        default:
            return false;
    }
    // Move events
    for(StepEvent ev : m_vEvents)
    {
        ev.setPosition(ev.getPosition() * fScale);
        ev.setDuration(ev.getDuration() * fScale);
    }
    return true;
}

uint32_t Pattern::getStepsPerBeat()
{
    return m_nStepsPerBeat;
}

void Pattern::setBeatsInPattern(uint32_t beats)
{
    if (beats > 0)
        m_nBeats = beats;

    // Remove steps if shrinking
    size_t nIndex = 0;
    for(; nIndex < m_vEvents.size(); ++nIndex)
        if(m_vEvents[nIndex].getPosition() >= (m_nBeats * m_nStepsPerBeat))
            break;
    m_vEvents.resize(nIndex);
}

uint32_t Pattern::getBeatsInPattern()
{
    return m_nBeats;
}

void Pattern::setScale(uint8_t scale)
{
    m_nScale = scale;
}

uint8_t Pattern::getScale()
{
    return m_nScale;
}

void Pattern::setTonic(uint8_t tonic)
{
    m_nTonic = tonic;
}

uint8_t Pattern::getTonic()
{
    return m_nTonic;
}

void Pattern::transpose(int value)
{
    // Check if any notes will be transposed out of MIDI note range (0..127)
    for(StepEvent ev : m_vEvents)
    {
        if(ev.getCommand() != MIDI_NOTE_ON)
            continue;
        int note = ev.getValue1start() + value;
        if(note > 127 || note < 0)
            return;
    }

    for(auto it = m_vEvents.begin(); it != m_vEvents.end(); ++it)
    {
        if(it->getCommand() != MIDI_NOTE_ON)
            continue;
        int note = it->getValue1start() + value;
        if(note > 127 || note < 0)
        {
            // Delete notes that have been pushed out of range
            //!@todo Should we squash notes that are out of range back in at ends? I don't think so.
            m_vEvents.erase(it);
        }
        else
        {
            it->setValue1start(note);
            it->setValue1end(note);
        }
    }
}

void Pattern::changeVelocityAll(int value)
{
    for(StepEvent ev : m_vEvents)
    {
        if(ev.getCommand() != MIDI_NOTE_ON)
            continue;
        int vel = ev.getValue2start() + value;
        if(vel > 127)
            vel = 127;
        if(vel < 1)
            vel = 1;
        ev.setValue2start(vel);
    }
}

void Pattern::changeDurationAll(float value)
{
    for(StepEvent ev : m_vEvents)
    {
        if(ev.getCommand() != MIDI_NOTE_ON)
            continue;
        float duration = ev.getDuration() + value;
        if(duration <= 0)
            return; // Don't allow jump larger than current value
        if(duration < 0.1) //!@todo How short should we allow duration change?
            duration = 0.1;
        ev.setDuration(duration);
    }
}

void Pattern::clear()
{
    m_vEvents.clear();
}

StepEvent* Pattern::getEventAt(uint32_t index)
{
    if(index >= m_vEvents.size())
        return NULL;
    return &(m_vEvents[index]);
}

int Pattern::getFirstEventAtStep(uint32_t step)
{
    int index;
    for(index = 0; index < m_vEvents.size(); ++index)
    {
        if(m_vEvents[index].getPosition() == step)
            return index;
    }
    return -1;
}

size_t Pattern::getEvents()
{
    return m_vEvents.size();
}

uint8_t Pattern::getRefNote()
{
    return m_nRefNote;
}

void Pattern::setRefNote(uint8_t note)
{
    if(note < 128)
        m_nRefNote = note;
}


uint32_t Pattern::getLastStep()
{
    if(m_vEvents.size() == 0)
        return -1;
    uint32_t nStep = 0;
    for(StepEvent ev : m_vEvents)
    {
        if(ev.getPosition() > nStep)
            nStep = ev.getPosition();
    }
    return nStep;
}
