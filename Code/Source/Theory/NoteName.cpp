#include "Theory/NoteName.h"

#include "Theory/NoteConvertor.h"

namespace theory
{

int NoteName::getPitchClass() const
{
    return NoteConvertor::parsePitchClass(rawNote);
}

}
