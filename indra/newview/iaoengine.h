#ifndef FS_IAOENGINE_H
#define FS_IAOENGINE_H

// Slim interface header for the Animation Override engine.
//
// This file is deliberately decoupled from the rest of AOEngine's machinery
// (LLSingleton, AOSet, inventory) so that unit tests can include IAOEngine
// without dragging in the full viewer build. Only LLUUID and std::string are
// pulled in.
//
// Production code that needs the full AOEngine should still include
// "aoengine.h". Code that only drives the engine (enable/disable, query
// folder, mouselook/sit) should depend on this interface plus AppContext.

#include "lluuid.h"
#include <string>

class IAOEngine
{
public:
    virtual ~IAOEngine() = default;

    virtual void enable(bool enable) = 0;
    virtual void enableStands(bool enable_stands) = 0;
    virtual LLUUID override(const LLUUID& motion, bool start) = 0;
    virtual void tick() = 0;
    virtual void update() = 0;
    virtual const LLUUID& getAOFolder() const = 0;
    virtual void inMouselook(bool mouselook) = 0;
    virtual void checkSitCancel() = 0;
    virtual void checkBelowWater(bool check_underwater) = 0;
    virtual const std::string getCurrentSetName() const = 0;
    virtual void saveSettings() = 0;
};

#endif // FS_IAOENGINE_H
