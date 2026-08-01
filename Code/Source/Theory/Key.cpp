#include "Theory/Key.h"

#include <stdexcept>

namespace theory
{

std::string getKeyJsonKey(Key key)
{
    switch (key)
    {
        case Key::C:  return "C";
        case Key::Db: return "Db";
        case Key::D:  return "D";
        case Key::Eb: return "Eb";
        case Key::E:  return "E";
        case Key::F:  return "F";
        case Key::Gb: return "Gb";
        case Key::G:  return "G";
        case Key::Ab: return "Ab";
        case Key::A:  return "A";
        case Key::Bb: return "Bb";
        case Key::B:  return "B";
    }

    return "C";
}

std::string getKeyLabel(Key key)
{
    return getKeyJsonKey(key);
}

Key parseKey(const std::string& jsonKey)
{
    if (jsonKey == "C")  return Key::C;
    if (jsonKey == "Db") return Key::Db;
    if (jsonKey == "D")  return Key::D;
    if (jsonKey == "Eb") return Key::Eb;
    if (jsonKey == "E")  return Key::E;
    if (jsonKey == "F")  return Key::F;
    if (jsonKey == "Gb") return Key::Gb;
    if (jsonKey == "G")  return Key::G;
    if (jsonKey == "Ab") return Key::Ab;
    if (jsonKey == "A")  return Key::A;
    if (jsonKey == "Bb") return Key::Bb;
    if (jsonKey == "B")  return Key::B;

    throw std::invalid_argument("Unknown key name: " + jsonKey);
}

}
