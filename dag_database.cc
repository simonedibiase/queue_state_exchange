#include "dag_database.h"

#include <unordered_map>

std::vector<Dag>
LoadDags()
{
    return {
        // DAG relativo a ALTAms
        Dag{{{"sink"},
             {"ATLAM5"},
             {"IPLSng"},
             {"KSCYng"},
             {"ATLAng"},
             {"ATLAng"},
             {"HSTNng", "IPLSng"},
             {"HSTNng"},
             {"CHINng", "WASHng"},
             {"LOSAng"},
             {"SNVAng", "DNVRng"},
             {"ATLAng"}}},

        // DAG relativo a ALTAng
        Dag{{
            {"ATLAng"},
            {"sink"},
            {"IPLSng", "NYCMng"},
            {"KSCYng"},
            {"ATLAng"},
            {"ATLAng"},
            {"IPLSng", "HSTNng"},
            {"HSTNng"},
            {"WASHng"},
            {"LOSAng"},
            {"SNVAng", "DNVRng"},
            {"ATLAng"},
        }},

        // DAG relativo a CHINng
        Dag{{{"ATLAng"},
             {"IPLSng", "WASHng"},
             {"sink"},
             {"KSCYng"},
             {"ATLAng"},
             {"CHINng"},
             {"IPLSng"},
             {"HSTNng"},
             {"CHINng"},
             {"LOSAng"},
             {"SNVAng", "DNVRng"},
             {"NYCMng"}}},

        // DAG relativo a DNVRng
        Dag{{{"ATLAng"},
             {"IPLSng", "HSTNng"},
             {"IPLSng"},
             {"sink"},
             {"LOSAng", "KSCYng"},
             {"KSCYng"},
             {"DNVRng"},
             {"SNVAng"},
             {"CHINng"},
             {"DNVRng", "STTLng"},
             {"DNVRng"},
             {"ATLAng", "NYCMng"}}},

        // DAG relativo a HSTNng
        Dag{{
            {"ATLAng"},
            {"HSTNng"},
            {"IPLSng", "NYCMng"},
            {"KSCYng", "SNVAng"},
            {"sink"},
            {"ATLAng"},
            {"HSTNng"},
            {"HSTNng"},
            {"WASHng"},
            {"LOSAng"},
            {"SNVAng", "DNVRng"},
            {"ATLAng"},
        }},

        // DAG relativo a IPLSng
        Dag{{
            {"ATLAng"},
            {"IPLSng"},
            {"IPLSng"},
            {"KSCYng"},
            {"KSCYng", "ATLAng"},
            {"sink"},
            {"IPLSng"},
            {"HSTNng"},
            {"CHINng"},
            {"LOSAng"},
            {"DNVRng", "SNVAng"},
            {"ATLAng", "NYCMng"},
        }},

        // DAG relativo a KSCYng
        Dag{{{"ATLAng"},
             {"IPLSng", "HSTNng"},
             {"IPLSng"},
             {"KSCYng"},
             {"KSCYng"},
             {"KSCYng"},
             {"sink"},
             {"SNVAng", "HSTNng"},
             {"CHINng"},
             {"STTLng", "DNVRng"},
             {"DNVRng"},
             {"ATLAng"}}},

        // DAG relativo a LOSAng
        Dag{{
            {"ATLAng"},
            {"HSTNng"},
            {"IPLSng"},
            {"SNVAng", "STTLng", "KSCYng"},
            {"LOSAng"},
            {"KSCYng"},
            {"HSTNng"},
            {"sink"},
            {"CHINng", "WASHng"},
            {"LOSAng"},
            {"SNVAng"},
            {"ATLAng"},
        }},

        // DAG relativo a NYCMng
        Dag{{{"ATLAng"},
             {"IPLSng", "WASHng"},
             {"NYCMng"},
             {"KSCYng"},
             {"KSCYng", "ATLAng"},
             {"CHINng"},
             {"IPLSng"},
             {"HSTNng"},
             {"sink"},
             {"DNVRng", "LOSAng"},
             {"SNVAng"},
             {"NYCMng"}}},

        // DAG relativo a SNVAng
        Dag{{{"ATLAng"},
             {"IPLSng", "HSTNng"},
             {"NYCMng"},
             {"SNVAng"},
             {"KSCYng", "LOSAng"},
             {"KSCYng"},
             {"DNVRng"},
             {"SNVAng"},
             {"WASHng"},
             {"sink"},
             {"SNVAng"},
             {"ATLAng"}}},

        // DAG relativo a STTLng
        Dag{{
            {"ATLAng"},
            {"HSTNng"},
            {"IPLSng"},
            {"STTLng"},
            {"LOSAng"},
            {"KSCYng"},
            {"DNVRng", "HSTNng"},
            {"SNVAng"},
            {"WASHng", "CHINng"},
            {"STTLng"},
            {"sink"},
            {"ATLAng"},
        }},

        // DAG relativo a WASHng
        Dag{{
            {"ATLAng"},
            {"WASHng"},
            {"NYCMng"},
            {"SNVAng", "KSCYng"},
            {"KSCYng", "ATLAng"},
            {"CHINng", "ATLAng"},
            {"IPLSng"},
            {"HSTNng"},
            {"WASHng"},
            {"LOSAng"},
            {"SNVAng"},
            {"sink"},
        }},

    };
}