#include "eas/same_message.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

namespace
{

struct CodeName
{
    const char* code;
    const char* name;
};

const CodeName kOrgs[] = {
    {"WXR", "Weather Radio / NOAA"},
    {"EAS", "Emergency Alert System"},
    {"CIV", "Civil authorities"},
    {"PEP", "Primary Entry Point"},
};

const SameEventInfo kEvents[] = {
    {"ADR", "Administrative Message", "Other"},
    {"AVA", "Avalanche Watch", "Watch"},
    {"AVW", "Avalanche Warning", "Warning"},
    {"BZW", "Blizzard Warning", "Warning"},
    {"CAE", "Child Abduction Emergency", "Warning"},
    {"CDW", "Civil Danger Warning", "Warning"},
    {"CEM", "Civil Emergency Message", "Warning"},
    {"CFA", "Coastal Flood Watch", "Watch"},
    {"CFW", "Coastal Flood Warning", "Warning"},
    {"DMO", "Practice/Demo Warning", "Test"},
    {"DSW", "Dust Storm Warning", "Warning"},
    {"EQW", "Earthquake Warning", "Warning"},
    {"EVI", "Evacuation Immediate", "Warning"},
    {"FFA", "Flash Flood Watch", "Watch"},
    {"FFS", "Flash Flood Statement", "Statement"},
    {"FFW", "Flash Flood Warning", "Warning"},
    {"FLA", "Flood Watch", "Watch"},
    {"FLS", "Flood Statement", "Statement"},
    {"FLW", "Flood Warning", "Warning"},
    {"FRW", "Fire Warning", "Warning"},
    {"HLS", "Hurricane Local Statement", "Statement"},
    {"HUA", "Hurricane Watch", "Watch"},
    {"HUW", "Hurricane Warning", "Warning"},
    {"HWA", "High Wind Watch", "Watch"},
    {"HWW", "High Wind Warning", "Warning"},
    {"LAE", "Local Area Emergency", "Warning"},
    {"LEW", "Law Enforcement Warning", "Warning"},
    {"NAT", "National Audible Test", "Test"},
    {"NIC", "National Information Center", "Other"},
    {"NMN", "Network Message Notification", "Other"},
    {"NPT", "National Periodic Test", "Test"},
    {"NUW", "Nuclear Power Plant Warning", "Warning"},
    {"RHW", "Radiological Hazard Warning", "Warning"},
    {"RMT", "Required Monthly Test", "Test"},
    {"RWT", "Required Weekly Test", "Test"},
    {"SMW", "Special Marine Warning", "Warning"},
    {"SPS", "Special Weather Statement", "Statement"},
    {"SPW", "Shelter in Place Warning", "Warning"},
    {"SSA", "Storm Surge Watch", "Watch"},
    {"SSW", "Storm Surge Warning", "Warning"},
    {"SVA", "Severe Thunderstorm Watch", "Watch"},
    {"SVR", "Severe Thunderstorm Warning", "Warning"},
    {"SVS", "Severe Weather Statement", "Statement"},
    {"TOA", "Tornado Watch", "Watch"},
    {"TOR", "Tornado Warning", "Warning"},
    {"TRA", "Tropical Storm Watch", "Watch"},
    {"TRW", "Tropical Storm Warning", "Warning"},
    {"TSA", "Tsunami Watch", "Watch"},
    {"TSW", "Tsunami Warning", "Warning"},
    {"VOW", "Volcano Warning", "Warning"},
    {"WSA", "Winter Storm Watch", "Watch"},
    {"WSW", "Winter Storm Warning", "Warning"},
    {"XHA", "Hazardous Materials Warning", "Warning"},
    {"XPD", "Air Quality Alert", "Advisory"},
};

const CodeName kStates[] = {
    {"01", "Alabama"},        {"02", "Alaska"},          {"04", "Arizona"},
    {"05", "Arkansas"},       {"06", "California"},      {"08", "Colorado"},
    {"09", "Connecticut"},    {"10", "Delaware"},        {"11", "District of Columbia"},
    {"12", "Florida"},        {"13", "Georgia"},         {"15", "Hawaii"},
    {"16", "Idaho"},          {"17", "Illinois"},        {"18", "Indiana"},
    {"19", "Iowa"},           {"20", "Kansas"},          {"21", "Kentucky"},
    {"22", "Louisiana"},      {"23", "Maine"},           {"24", "Maryland"},
    {"25", "Massachusetts"},  {"26", "Michigan"},        {"27", "Minnesota"},
    {"28", "Mississippi"},    {"29", "Missouri"},        {"30", "Montana"},
    {"31", "Nebraska"},       {"32", "Nevada"},          {"33", "New Hampshire"},
    {"34", "New Jersey"},     {"35", "New Mexico"},      {"36", "New York"},
    {"37", "North Carolina"}, {"38", "North Dakota"},    {"39", "Ohio"},
    {"40", "Oklahoma"},       {"41", "Oregon"},          {"42", "Pennsylvania"},
    {"44", "Rhode Island"},   {"45", "South Carolina"},  {"46", "South Dakota"},
    {"47", "Tennessee"},      {"48", "Texas"},           {"49", "Utah"},
    {"50", "Vermont"},        {"51", "Virginia"},        {"53", "Washington"},
    {"54", "West Virginia"},  {"55", "Wisconsin"},       {"56", "Wyoming"},
    {"60", "American Samoa"}, {"66", "Guam"},            {"69", "Northern Mariana Islands"},
    {"72", "Puerto Rico"},    {"78", "U.S. Virgin Islands"},
};

const char* lookup(const CodeName* table, size_t n, const std::string& code)
{
    for (size_t i = 0; i < n; ++i)
    {
        if (code == table[i].code)
        {
            return table[i].name;
        }
    }
    return nullptr;
}

std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> out;
    std::string              cur;
    for (char c : s)
    {
        if (c == delim)
        {
            out.push_back(cur);
            cur.clear();
        }
        else
        {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

std::string jsonEscape(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

} // namespace

bool parseSameHeader(const std::string& raw, SameMessage& m)
{
    if (raw.compare(0, 5, "ZCZC-") != 0)
    {
        return false;
    }

    size_t pos = 5;
    size_t dash = raw.find('-', pos);
    if (dash == std::string::npos)
    {
        return false;
    }
    m.org = raw.substr(pos, dash - pos);
    pos   = dash + 1;

    dash = raw.find('-', pos);
    if (dash == std::string::npos)
    {
        return false;
    }
    m.event = raw.substr(pos, dash - pos);
    pos     = dash + 1;

    size_t plus = raw.find('+', pos);
    if (plus == std::string::npos)
    {
        return false;
    }
    for (const std::string& loc : split(raw.substr(pos, plus - pos), '-'))
    {
        if (!loc.empty())
        {
            m.locations.push_back(loc);
        }
    }
    pos = plus + 1;

    if (raw.size() >= pos + 4)
    {
        m.purgeMinutes = std::atoi(raw.substr(pos, 4).c_str());
    }
    pos += 4;
    if (pos < raw.size() && raw[pos] == '-')
    {
        ++pos;
    }

    m.julianTime = raw.substr(pos, 7);
    pos += 7;
    if (pos < raw.size() && raw[pos] == '-')
    {
        ++pos;
    }

    size_t end = raw.find('-', pos);
    size_t len = (end == std::string::npos) ? raw.size() - pos : end - pos;
    if (len > 8)
    {
        len = 8;
    }
    m.callsign = raw.substr(pos, len);
    while (!m.callsign.empty() && (m.callsign.back() == '-' || m.callsign.back() == ' '))
    {
        m.callsign.pop_back();
    }

    m.raw   = raw;
    m.valid = !m.org.empty() && !m.event.empty() && !m.locations.empty() && m.julianTime.size() == 7 &&
              !m.callsign.empty();
    return m.valid;
}

const char* sameOrgName(const std::string& code)
{
    const char* n = lookup(kOrgs, sizeof(kOrgs) / sizeof(kOrgs[0]), code);
    return n ? n : "Unknown";
}

const char* sameEventName(const std::string& code)
{
    for (const SameEventInfo& e : kEvents)
    {
        if (code == e.code)
        {
            return e.name;
        }
    }
    return "Unknown event";
}

const SameEventInfo* sameEventList(int& count)
{
    count = (int)(sizeof(kEvents) / sizeof(kEvents[0]));
    return kEvents;
}

const char* sameEventCategory(const std::string& code)
{
    for (const SameEventInfo& e : kEvents)
    {
        if (code == e.code)
        {
            return e.category;
        }
    }
    return "Other";
}

std::string sameLocationName(const std::string& code)
{
    if (code.size() != 6)
    {
        return code;
    }

    const char* name = lookup(kStates, sizeof(kStates) / sizeof(kStates[0]), code.substr(1, 2));
    if (!name)
    {
        name = lookup(kStates, sizeof(kStates) / sizeof(kStates[0]), code.substr(0, 2));
    }

    std::string out = code;
    if (name)
    {
        out += " (";
        out += name;
        out += ")";
    }
    return out;
}

std::string sameFormatMessage(const SameMessage& m)
{
    std::ostringstream ss;
    ss << "\n" << m.raw << "\n";
    ss << "  Originator : " << m.org << " - " << sameOrgName(m.org) << "\n";
    ss << "  Event      : " << m.event << " - " << sameEventName(m.event) << "\n";
    ss << "  Locations  : ";
    for (size_t i = 0; i < m.locations.size(); ++i)
    {
        if (i)
        {
            ss << ", ";
        }
        ss << sameLocationName(m.locations[i]);
    }
    ss << "\n";
    ss << "  Purge      : " << m.purgeMinutes << " min\n";
    ss << "  Issued     : day " << m.julianTime.substr(0, 3) << " " << m.julianTime.substr(3, 2) << ":"
       << m.julianTime.substr(5, 2) << " UTC\n";
    ss << "  Callsign   : " << m.callsign << "\n";
    ss << "  Copies     : " << m.repeats << "\n";
    return ss.str();
}

std::string sameCanonicalKey(const SameMessage& m)
{
    std::ostringstream ss;
    ss << m.org << '|' << m.event << '|';
    for (const std::string& loc : m.locations)
    {
        ss << loc << ',';
    }
    ss << '|' << m.purgeMinutes << '|' << m.julianTime << '|' << m.callsign;
    return ss.str();
}

std::string sameJsonMessage(const SameMessage& m)
{
    std::ostringstream ss;
    ss << "{\"type\":\"same\",\"raw\":\"" << jsonEscape(m.raw) << "\",\"org\":\"" << jsonEscape(m.org)
       << "\",\"orgName\":\"" << sameOrgName(m.org) << "\",\"event\":\"" << jsonEscape(m.event) << "\",\"eventName\":\""
       << sameEventName(m.event) << "\",\"locations\":[";
    for (size_t i = 0; i < m.locations.size(); ++i)
    {
        if (i)
        {
            ss << ",";
        }
        ss << "\"" << jsonEscape(m.locations[i]) << "\"";
    }
    ss << "],\"purgeMinutes\":" << m.purgeMinutes << ",\"julianTime\":\"" << jsonEscape(m.julianTime)
       << "\",\"callsign\":\"" << jsonEscape(m.callsign) << "\",\"repeats\":" << m.repeats << "}";
    return ss.str();
}
