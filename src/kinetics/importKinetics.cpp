/**
 *  @file importKinetics.cpp
 *     Declarations of global routines for the importing
 *     of kinetics data from XML files (see \ref inputfiles).
 *
 *     This file contains routines which are global routines, i.e.,
 *     not part of any object. These routine take as input, ctml
 *     pointers to data, and pointers to %Cantera objects. The purpose
 *     of these routines is to initialize the %Cantera objects with data
 *     from the ctml tree structures.
 */
// Copyright 2002  California Institute of Technology

#include "cantera/kinetics/importKinetics.h"
#include "cantera/thermo/ThermoFactory.h"
#include "cantera/kinetics/Reaction.h"
#include "cantera/base/stringUtils.h"
#include "cantera/base/ctml.h"

#include <cstring>

using namespace std;

namespace Cantera
{

bool installReactionArrays(const XML_Node& p, Kinetics& kin,
                           std::string default_phase, bool check_for_duplicates)
{
    int itot = 0;

    // Search the children of the phase element for the XML element named
    // reactionArray. If we can't find it, then return signaling having not
    // found any reactions. Apparently, we allow multiple reactionArray elements
    // here Each one will be processed sequentially, with the end result being
    // purely additive.
    vector<XML_Node*> rarrays = p.getChildren("reactionArray");
    if (rarrays.empty()) {
        kin.finalize();
        return false;
    }
    for (size_t n = 0; n < rarrays.size(); n++) {
        // Go get a reference to the current XML element, reactionArray. We will
        // process this element now.
        const XML_Node& rxns = *rarrays[n];

        // The reactionArray element has an attribute called, datasrc. The value
        // of the attribute is the XML element comprising the top of the tree of
        // reactions for the phase. Find this datasrc element starting with the
        // root of the current XML node.
        const XML_Node* rdata = get_XML_Node(rxns["datasrc"], &rxns.root());

        // If the reactionArray element has a child element named "skip", and if
        // the attribute of skip called "species" has a value of "undeclared",
        // we will set rxnrule.skipUndeclaredSpecies to 'true'. rxnrule is
        // passed to the routine that parses each individual reaction so that
        // the parser will skip all reactions containing an undefined species
        // without throwing an error.
        //
        // Similarly, an attribute named "third_bodies" with the value of
        // "undeclared" will skip undeclared third body efficiencies (while
        // retaining the reaction and any other efficiencies).
        if (rxns.hasChild("skip")) {
            const XML_Node& sk = rxns.child("skip");
            if (sk["species"] == "undeclared") {
                kin.skipUndeclaredSpecies(true);
            }
            if (sk["third_bodies"] == "undeclared") {
                kin.skipUndeclaredThirdBodies(true);
            }
        }

        // Search for child elements called include. We only include a reaction
        // if it's tagged by one of the include fields. Or, we include all
        // reactions if there are no include fields.
        vector<XML_Node*> incl = rxns.getChildren("include");
        vector<XML_Node*> allrxns = rdata->getChildren("reaction");
        // if no 'include' directive, then include all reactions
        if (incl.empty()) {
            for (size_t i = 0; i < allrxns.size(); i++) {
                kin.addReaction(newReaction(*allrxns[i]));
                ++itot;
            }
        } else {
            for (size_t nii = 0; nii < incl.size(); nii++) {
                const XML_Node& ii = *incl[nii];
                string imin = ii["min"];
                string imax = ii["max"];

                string::size_type iwild = string::npos;
                if (imax == imin) {
                    iwild = imin.find("*");
                    if (iwild != string::npos) {
                        imin = imin.substr(0,iwild);
                        imax = imin;
                    }
                }

                for (size_t i = 0; i < allrxns.size(); i++) {
                    const XML_Node* r = allrxns[i];
                    string rxid;
                    if (r) {
                        rxid = r->attrib("id");
                        if (iwild != string::npos) {
                            rxid = rxid.substr(0,iwild);
                        }

                        // To decide whether the reaction is included or not we
                        // do a lexical min max and operation. This sometimes
                        // has surprising results.
                        if ((rxid >= imin) && (rxid <= imax)) {
                            kin.addReaction(newReaction(*r));
                            ++itot;
                        }
                    }
                }
            }
        }
    }

    if (check_for_duplicates) {
        kin.checkDuplicates();
    }

    // Finalize the installation of the kinetics, now that we know the true
    // number of reactions in the mechanism, itot.
    kin.finalize();
    return true;
}

bool importKinetics(const XML_Node& phase, std::vector<ThermoPhase*> th,
                    Kinetics* k)
{
    if (k == 0) {
        return false;
    }

    // This phase will be the owning phase for the kinetics operator
    // For interfaces, it is the surface phase between two volumes.
    // For homogeneous kinetics, it's the current volumetric phase.
    string owning_phase = phase["id"];

    bool check_for_duplicates = false;
    if (phase.parent() && phase.parent()->hasChild("validate")) {
        const XML_Node& d = phase.parent()->child("validate");
        if (d["reactions"] == "yes") {
            check_for_duplicates = true;
        }
    }

    // if other phases are involved in the reaction mechanism, they must be
    // listed in a 'phaseArray' child element. Homogeneous mechanisms do not
    // need to include a phaseArray element.
    vector<string> phase_ids;
    if (phase.hasChild("phaseArray")) {
        const XML_Node& pa = phase.child("phaseArray");
        getStringArray(pa, phase_ids);
    }
    phase_ids.push_back(owning_phase);

    int np = static_cast<int>(phase_ids.size());
    int nt = static_cast<int>(th.size());

    // for each referenced phase, attempt to find its id among those
    // phases specified.
    bool phase_ok;
    string phase_id;
    string msg = "";
    for (int n = 0; n < np; n++) {
        phase_id = phase_ids[n];
        phase_ok = false;
        // loop over the supplied 'ThermoPhase' objects representing
        // phases, to find an object with the same id.
        for (int m = 0; m < nt; m++) {
            if (th[m]->id() == phase_id) {
                phase_ok = true;
                // if no phase with this id has been added to
                //the kinetics manager yet, then add this one
                if (k->phaseIndex(phase_id) == npos) {
                    k->addPhase(*th[m]);
                }
            }
            msg += " "+th[m]->id();
        }
        if (!phase_ok) {
            throw CanteraError("importKinetics",
                               "phase "+phase_id+" not found. Supplied phases are:"+msg);
        }
    }

    // allocates arrays, etc. Must be called after the phases have been added to
    // 'kin', so that the number of species in each phase is known.
    k->init();

    // Install the reactions.
    return installReactionArrays(phase, *k, owning_phase, check_for_duplicates);
}

bool buildSolutionFromXML(XML_Node& root, const std::string& id,
                          const std::string& nm, ThermoPhase* th, Kinetics* kin)
{
    XML_Node* x;
    x = get_XML_NameID(nm, string("#")+id, &root);
    if (!x) {
        return false;
    }

    // Fill in the ThermoPhase object by querying the const XML_Node tree
    // located at x.
    importPhase(*x, th);

    // Create a vector of ThermoPhase pointers of length 1 having the current th
    // ThermoPhase as the entry.
    std::vector<ThermoPhase*> phases(1);
    phases[0] = th;

    // Fill in the kinetics object k, by querying the const XML_Node tree
    // located by x. The source terms and eventually the source term vector will
    // be constructed from the list of ThermoPhases in the vector, phases.
    importKinetics(*x, phases, kin);
    return true;
}

}
