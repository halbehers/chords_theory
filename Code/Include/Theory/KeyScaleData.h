#pragma once

#include <array>
#include <string>
#include <vector>

#include "Theory/Key.h"
#include "Theory/NoteName.h"
#include "Theory/Scale.h"
#include "Theory/ScaleDegreeData.h"

namespace theory
{

struct KeyScaleData
{
    Key key = Key::C;
    Scale scale = Scale::Major;
    std::array<std::string, 5> moods {};
    std::vector<NoteName> scaleNotes;     // 7 notes, or 6 for Scale::MinorBlues
    std::vector<ScaleDegreeData> degrees; // 7 entries, or 3 (I, IV, V only) for Scale::MinorBlues

    [[nodiscard]] bool isMinorBlues() const { return scale == Scale::MinorBlues; }

    // nullptr if this degree doesn't exist for this scale (e.g. II/III/VI/VII on Minor Blues).
    [[nodiscard]] const ScaleDegreeData* findDegree(Degree degreeToFind) const;
};

}
