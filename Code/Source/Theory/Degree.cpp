#include "Theory/Degree.h"

#include <stdexcept>

namespace theory
{

std::string getDegreeLabel(Degree degree)
{
    switch (degree)
    {
        case Degree::I:   return "I";
        case Degree::II:  return "II";
        case Degree::III: return "III";
        case Degree::IV:  return "IV";
        case Degree::V:   return "V";
        case Degree::VI:  return "VI";
        case Degree::VII: return "VII";
    }

    return "I";
}

Degree parseDegree(const std::string& label)
{
    if (label == "I")   return Degree::I;
    if (label == "II")  return Degree::II;
    if (label == "III") return Degree::III;
    if (label == "IV")  return Degree::IV;
    if (label == "V")   return Degree::V;
    if (label == "VI")  return Degree::VI;
    if (label == "VII") return Degree::VII;

    throw std::invalid_argument("Unknown degree label: " + label);
}

}
