/**
 * server_maplejuice.h
 * Define maplejuice contents used in server.
 */

#ifndef SERVER_MAPLEJUICE_H
#define SERVER_MAPLEJUICE_H

#include <vector>

/// Enumerator for worker phase stage.
enum Stage {
    PHASE_I, PHASE_II, PHASE_III, PHASE_IV
};

/// Struct for Maple Mission.
class MapleMission {
public:
    int mission_id;
    Stage phase_id;
    vector<string> files;
};

/// Struct for Juice Mission.
class JuiceMission {
public:
    int mission_id;
    Stage phase_id;
    vector<string> prefixes;
};


#endif //SERVER_MAPLEJUICE_H
