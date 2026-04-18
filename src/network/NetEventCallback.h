#ifndef _MINECRAFT_NETWORK_NETEVENTCALLBACK_H_
#define _MINECRAFT_NETWORK_NETEVENTCALLBACK_H_

// VoxelForge: Multiplayer removed. NetEventCallback kept as empty base for compatibility.

class Level;

class NetEventCallback
{
public:
    virtual void levelGenerated(Level* level) {}
    virtual ~NetEventCallback() {}
};

#endif
