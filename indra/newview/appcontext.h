#ifndef FS_APPCONTEXT_H
#define FS_APPCONTEXT_H

// Forward declarations only — including this header costs nothing.
class IAOEngine;
class IContactSets;
class IFSRadar;
class IFSLSLBridge;

// AppContext holds interface pointers to Firestorm-specific subsystems.
// Production code: pointers are populated by each subsystem's initSingleton().
// Test code: replace individual pointers with mock implementations before
// exercising the code under test — no LLSingleton required.
struct AppContext
{
    IAOEngine*    aoEngine{};
    IContactSets* contactSets{};
    IFSRadar*     radar{};
    IFSLSLBridge* lslBridge{};
};

extern AppContext gAppContext;

#endif // FS_APPCONTEXT_H
