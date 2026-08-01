#include "Theory/KeyScaleData.h"

namespace theory
{

const ScaleDegreeData* KeyScaleData::findDegree(Degree degreeToFind) const
{
    for (const auto& degreeData : degrees)
    {
        if (degreeData.degree == degreeToFind)
            return &degreeData;
    }

    return nullptr;
}

}
