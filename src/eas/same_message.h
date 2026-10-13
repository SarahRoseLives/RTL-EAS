// SAME (Specific Area Message Encoding) message model and lookup tables.
//
// Wire header format (ASCII, sent three times, each preceded by a 16-byte
// 0xAB preamble):
//
//   ZCZC-ORG-EEE-PSSCCC-PSSCCC...+TTTT-JJJHHMM-LLLLLLLL-
//
//   ORG    originator (WXR, EAS, CIV, PEP ...)
//   EEE    event code (RWT, TOR, SVR ...)
//   PSSCCC location: P=subdivision, SS=FIPS state, CCC=county
//   TTTT   purge/valid time in minutes
//   JJJHHMM  Julian day + UTC time of issue
//   LLLLLLLL station callsign / identifier
#pragma once

#include <string>
#include <vector>

struct SameMessage
{
    std::string              raw;
    std::string              org;
    std::string              event;
    std::vector<std::string> locations;
    int                      purgeMinutes = 0;
    std::string              julianTime;
    std::string              callsign;
    int                      repeats = 1;
    bool                     valid   = false;
};

// Parses a raw "ZCZC-..." string. Returns false if it is not a well-formed
// SAME header.
bool parseSameHeader(const std::string& raw, SameMessage& out);

const char* sameOrgName(const std::string& code);
const char* sameEventName(const std::string& code);

// Full event table for the subscription UI.
struct SameEventInfo
{
    const char* code;
    const char* name;
    const char* category; // "Warning", "Watch", "Advisory", "Statement", "Test", "Other"
};
const SameEventInfo* sameEventList(int& count);
const char*          sameEventCategory(const std::string& code);

// "039007" -> "039007 (Ohio)"; unknown state codes fall back to the raw code.
std::string sameLocationName(const std::string& code);

// Multi-line human-readable rendering (leading newline included).
std::string sameFormatMessage(const SameMessage& m);

// Single-line machine-readable JSON object.
std::string sameJsonMessage(const SameMessage& m);

// Field-based identity used to group repeated transmissions of one header.
std::string sameCanonicalKey(const SameMessage& m);
