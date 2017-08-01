#include "gtest/gtest.h"
#include "cantera/thermo/ThermoFactory.h"
#include "cantera/thermo/PDSSFactory.h"
#include "cantera/thermo/PDSS_ConstVol.h"
#include "cantera/thermo/PDSS_Water.h"
#include "cantera/thermo/FixedChemPotSSTP.h"
#include "cantera/thermo/PureFluidPhase.h"
#include "cantera/thermo/WaterSSTP.h"
#include "cantera/thermo/RedlichKwongMFTP.h"
#include "cantera/thermo/IonsFromNeutralVPSSTP.h"
#include "cantera/thermo/IdealSolnGasVPSS.h"
#include "cantera/thermo/IdealMolalSoln.h"
#include "cantera/thermo/DebyeHuckel.h"
#include "cantera/thermo/MargulesVPSSTP.h"
#include "cantera/thermo/LatticePhase.h"
#include "cantera/thermo/StoichSubstance.h"
#include "cantera/thermo/LatticeSolidPhase.h"
#include "cantera/thermo/IdealSolidSolnPhase.h"

#include "cantera/thermo/NasaPoly2.h"
#include "cantera/thermo/ConstCpPoly.h"
#include "cantera/thermo/ShomatePoly.h"
#include "cantera/thermo/IdealGasPhase.h"
#include "cantera/thermo/Mu0Poly.h"
#include "cantera/base/ctml.h"
#include "cantera/base/stringUtils.h"
#include <fstream>
#include "thermo_data.h"

namespace Cantera
{

shared_ptr<Species> make_species(const std::string& name,
     const std::string& composition, const double* nasa_coeffs)
{
    auto species = make_shared<Species>(name, parseCompString(composition));
    species->thermo.reset(new NasaPoly2(200, 3500, 101325, nasa_coeffs));
    return species;
}

shared_ptr<Species> make_shomate_species(const std::string& name,
     const std::string& composition, const double* shomate_coeffs)
{
    auto species = make_shared<Species>(name, parseCompString(composition));
    species->thermo.reset(new ShomatePoly(200, 3500, 101325, shomate_coeffs));
    return species;
}

shared_ptr<Species> make_shomate2_species(const std::string& name,
     const std::string& composition, const double* shomate_coeffs)
{
    auto species = make_shared<Species>(name, parseCompString(composition));
    species->thermo.reset(new ShomatePoly2(200, 3500, 101325, shomate_coeffs));
    return species;
}

shared_ptr<Species> make_species(const std::string& name,
    const std::string& composition, double h298,
    double T1, double mu1, double T2, double mu2)
{
    auto species = make_shared<Species>(name, parseCompString(composition));
    double coeffs[] = {2, h298, T1, mu1*GasConstant*T1, T2, mu2*GasConstant*T2};
    species->thermo.reset(new Mu0Poly(200, 3500, 101325, coeffs));
    return species;
}

shared_ptr<Species> make_const_cp_species(const std::string& name,
    const std::string& composition, double T0, double h0, double s0, double cp)
{
    auto species = make_shared<Species>(name, parseCompString(composition));
    double coeffs[] = {T0, h0, s0, cp};
    species->thermo.reset(new ConstCpPoly(200, 3500, 101325, coeffs));
    return species;
}


class FixedChemPotSstpConstructorTest : public testing::Test
{
};

TEST_F(FixedChemPotSstpConstructorTest, fromXML)
{
    std::unique_ptr<ThermoPhase> p(newPhase("../data/LiFixed.xml"));
    ASSERT_EQ((int) p->nSpecies(), 1);
    double mu;
    p->getChemPotentials(&mu);
    ASSERT_DOUBLE_EQ(-2.3e7, mu);
}

TEST_F(FixedChemPotSstpConstructorTest, SimpleConstructor)
{
    FixedChemPotSSTP p("Li", -2.3e7);
    ASSERT_EQ((int) p.nSpecies(), 1);
    double mu;
    p.getChemPotentials(&mu);
    ASSERT_DOUBLE_EQ(-2.3e7, mu);
}

TEST(IonsFromNeutralConstructor, fromXML)
{
    std::unique_ptr<ThermoPhase> p(newPhase("../data/mock_ion.xml",
                                            "mock_ion_phase"));
    ASSERT_EQ((int) p->nSpecies(), 2);
    p->setState_TPX(500, 2e5, "K+:0.1, Cl-:0.1");
    vector_fp mu(p->nSpecies());
    p->getChemPotentials(mu.data());

    // Values for regression testing only -- no reference values known for comparison
    EXPECT_NEAR(p->density(), 1984.3225978174073, 1e-6);
    EXPECT_NEAR(p->enthalpy_mass(), -8035317241137.971, 1e-1);
    EXPECT_NEAR(mu[0], -4.66404010e+08, 1e1);
    EXPECT_NEAR(mu[1], -2.88157298e+06, 1e-1);
}

#ifndef HAS_NO_PYTHON // skip these tests if the Python converter is unavailable
class CtiConversionTest : public testing::Test
{
public:
    CtiConversionTest() {
        appdelete();
    }

    std::unique_ptr<ThermoPhase> p1;
    std::unique_ptr<ThermoPhase> p2;
    void compare()
    {
        ASSERT_EQ(p1->nSpecies(), p2->nSpecies());
        for (size_t i = 0; i < p1->nSpecies(); i++) {
            ASSERT_EQ(p1->speciesName(i), p2->speciesName(i));
            ASSERT_EQ(p1->molecularWeight(i), p2->molecularWeight(i));
        }
    }
};

TEST_F(CtiConversionTest, ExplicitConversion) {
    p1.reset(newPhase("../data/air-no-reactions.xml"));
    ct2ctml("../data/air-no-reactions.cti");
    p2.reset(newPhase("air-no-reactions.xml", ""));
    compare();
}

TEST_F(CtiConversionTest, ImplicitConversion) {
    p1.reset(newPhase("../data/air-no-reactions.xml"));
    p2.reset(newPhase("../data/air-no-reactions.cti"));
    compare();
}

class ChemkinConversionTest : public testing::Test {
public:
    void copyInputFile(const std::string& name) {
        std::string in_name = "../data/" + name;
        std::ifstream source(in_name, std::ios::binary);
        std::ofstream dest(name, std::ios::binary);
        dest << source.rdbuf();
    }
};

TEST_F(ChemkinConversionTest, ValidConversion) {
    copyInputFile("pdep-test.inp");
    ck2cti("pdep-test.inp");
    std::unique_ptr<ThermoPhase> p(newPhase("pdep-test.cti"));
    ASSERT_GT(p->temperature(), 0.0);
}

TEST_F(ChemkinConversionTest, MissingInputFile) {
    ASSERT_THROW(ck2cti("nonexistent-file.inp"),
                 CanteraError);
}

TEST_F(ChemkinConversionTest, FailedConversion) {
    copyInputFile("h2o2_missingThermo.inp");
    ASSERT_THROW(ck2cti("h2o2_missingThermo.inp"),
                 CanteraError);
}
#endif

class ConstructFromScratch : public testing::Test
{
public:
    ConstructFromScratch()
        : sH2O(make_species("H2O", "H:2 O:1", h2o_nasa_coeffs))
        , sH2(make_species("H2", "H:2", h2_nasa_coeffs))
        , sO2(make_species("O2", "O:2", o2_nasa_coeffs))
        , sOH(make_species("OH", "H:1 O:1", oh_nasa_coeffs))
        , sCO(make_species("CO", "C:1 O:1", o2_nasa_coeffs))
        , sCO2(new Species("CO2", parseCompString("C:1 O:2")))
    {
        sCO2->thermo.reset(new ShomatePoly2(200, 3500, 101325, co2_shomate_coeffs));
    }

    shared_ptr<Species> sH2O, sH2, sO2, sOH, sCO, sCO2;
};

TEST_F(ConstructFromScratch, AddElements)
{
    IdealGasPhase p;
    p.addElement("H");
    p.addElement("O");
    ASSERT_EQ((size_t) 2, p.nElements());
    ASSERT_EQ("H", p.elementName(0));
    ASSERT_EQ((size_t) 1, p.elementIndex("O"));
}

TEST_F(ConstructFromScratch, AddSpeciesDefaultBehavior)
{
    IdealGasPhase p;
    p.addElement("H");
    p.addElement("O");
    p.addSpecies(sH2O);
    p.addSpecies(sH2);

    ASSERT_EQ((size_t) 2, p.nSpecies());

    p.addSpecies(sO2);
    p.addSpecies(sOH);

    ASSERT_EQ((size_t) 4, p.nSpecies());
    ASSERT_EQ("H2", p.speciesName(1));
    ASSERT_EQ(2, p.nAtoms(2, 1)); // O in O2
    ASSERT_EQ(2, p.nAtoms(0, 0)); // H in H2O
    ASSERT_THROW(p.addSpecies(sCO), CanteraError);
}

TEST_F(ConstructFromScratch, ignoreUndefinedElements)
{
    IdealGasPhase p;
    p.addElement("H");
    p.addElement("O");
    p.ignoreUndefinedElements();

    p.addSpecies(sO2);
    p.addSpecies(sOH);
    ASSERT_EQ((size_t) 2, p.nSpecies());

    p.addSpecies(sCO);
    p.addSpecies(sCO2);
    ASSERT_EQ((size_t) 2, p.nSpecies());
    ASSERT_EQ((size_t) 2, p.nElements());
    ASSERT_EQ(npos, p.speciesIndex("CO2"));
}

TEST_F(ConstructFromScratch, addUndefinedElements)
{
    IdealGasPhase p;
    p.addElement("H");
    p.addElement("O");
    p.addUndefinedElements();

    p.addSpecies(sH2);
    p.addSpecies(sOH);
    ASSERT_EQ((size_t) 2, p.nSpecies());
    ASSERT_EQ((size_t) 2, p.nElements());

    p.addSpecies(sCO);
    p.addSpecies(sCO2);
    ASSERT_EQ((size_t) 4, p.nSpecies());
    ASSERT_EQ((size_t) 3, p.nElements());
    ASSERT_EQ((size_t) 1, p.nAtoms(p.speciesIndex("CO2"), p.elementIndex("C")));
    ASSERT_EQ((size_t) 2, p.nAtoms(p.speciesIndex("co2"), p.elementIndex("O")));
    p.setMassFractionsByName("H2:0.5, CO2:0.5");
    ASSERT_DOUBLE_EQ(0.5, p.massFraction("CO2"));
}

TEST_F(ConstructFromScratch, RedlichKwongMFTP)
{
    RedlichKwongMFTP p;
    p.addUndefinedElements();
    p.addSpecies(sCO2);
    p.addSpecies(sH2O);
    p.addSpecies(sH2);
    double fa = toSI("bar-cm6/mol2");
    double fb = toSI("cm3/mol");
    p.setBinaryCoeffs("H2", "H2O", 4 * fa, 40 * fa);
    p.setSpeciesCoeffs("CO2", 7.54e7 * fa, -4.13e4 * fa, 27.80 * fb);
    p.setBinaryCoeffs("CO2", "H2O", 7.897e7 * fa, 0.0);
    p.setSpeciesCoeffs("H2O", 1.7458e8 * fa, -8e4 * fa, 18.18 * fb);
    p.setSpeciesCoeffs("H2", 30e7 * fa, -330e4 * fa, 31 * fb);
    p.initThermo();
    p.setMoleFractionsByName("CO2:0.9998, H2O:0.0002");
    p.setState_TP(300, 200 * OneAtm);
    EXPECT_NEAR(p.pressure(), 200 * OneAtm, 1e-5);
    // Arbitrary regression test values
    EXPECT_NEAR(p.density(), 892.421, 2e-3);
    EXPECT_NEAR(p.enthalpy_mole(), -404848642.3797, 1e-3);
}

TEST_F(ConstructFromScratch, IdealSolnGasVPSS_gas)
{
    IdealSolnGasVPSS p;
    p.addUndefinedElements();
    p.addSpecies(sH2O);
    p.addSpecies(sH2);
    p.addSpecies(sO2);
    std::unique_ptr<PDSS> pH2O(newPDSS("ideal-gas"));
    std::unique_ptr<PDSS> pH2(newPDSS("ideal-gas"));
    std::unique_ptr<PDSS> pO2(newPDSS("ideal-gas"));
    p.installPDSS(0, std::move(pH2O));
    p.installPDSS(1, std::move(pH2));
    p.installPDSS(2, std::move(pO2));

    p.setGasMode();
    EXPECT_THROW(p.setStandardConcentrationModel("unity"), CanteraError);
    p.initThermo();

    p.setState_TPX(400, 5*OneAtm, "H2:0.01, O2:0.99");
    p.equilibrate("HP");

    EXPECT_NEAR(p.temperature(), 479.929, 1e-3); // based on h2o2.cti
    EXPECT_NEAR(p.moleFraction("H2O"), 0.01, 1e-4);
    EXPECT_NEAR(p.moleFraction("H2"), 0.0, 1e-4);
}

TEST(PureFluidFromScratch, CarbonDioxide)
{
    PureFluidPhase p;
    auto sCO2 = make_shared<Species>("CO2", parseCompString("C:1 O:2"));
    sCO2->thermo.reset(new ShomatePoly2(200, 6000, 101325, co2_shomate_coeffs));
    p.addUndefinedElements();
    p.addSpecies(sCO2);
    p.setSubstance("carbondioxide");
    p.initThermo();
    p.setState_Tsat(280, 0.5);
    EXPECT_NEAR(p.pressure(), 4160236.987, 1e-2);
}

TEST(WaterSSTP, fromScratch)
{
    WaterSSTP water;
    water.addUndefinedElements();
    water.addSpecies(make_species("H2O", "H:2, O:1", h2o_nasa_coeffs));
    water.initThermo();
    water.setState_TP(298.15, 1e5);
    EXPECT_NEAR(water.enthalpy_mole() / 1e6, -285.83, 2e-2);
}

TEST(IdealMolalSoln, fromScratch)
{
    IdealMolalSoln p;
    p.addUndefinedElements();
    p.addSpecies(make_species("H2O(l)", "H:2, O:1", h2_nasa_coeffs));
    p.addSpecies(make_species("CO2(aq)", "C:1, O:2", h2_nasa_coeffs));
    p.addSpecies(make_species("H2S(aq)", "H:2, S:1", h2_nasa_coeffs));
    p.addSpecies(make_species("CH4(aq)", "C:1, H:4", h2_nasa_coeffs));
    size_t k = 0;
    for (double v : {1.5, 1.3, 0.1, 0.1}) {
        std::unique_ptr<PDSS_ConstVol> ss(new PDSS_ConstVol());
        ss->setMolarVolume(v);
        p.installPDSS(k++, std::move(ss));
    }
    p.setStandardConcentrationModel("solvent_volume");
    p.setCutoffModel("polyexp");
    // These propreties probably shouldn't be public
    p.IMS_X_o_cutoff_ = 0.20;
    p.IMS_gamma_o_min_ = 0.00001;
    p.IMS_gamma_k_min_ = 10.0;
    p.IMS_slopefCut_ = 0.6;
    p.IMS_slopegCut_ = 0.0;
    p.IMS_cCut_ = .05;
    p.initThermo();
    p.setState_TPM(298.15, OneAtm, "CH4(aq):0.01, H2S(aq):0.03, CO2(aq):0.1");

    EXPECT_NEAR(p.enthalpy_mole(), 0.013282, 1e-6);
    EXPECT_NEAR(p.gibbs_mole(), -3.8986e7, 1e3);
    EXPECT_NEAR(p.density(), 12.058, 1e-3);
}

TEST(DebyeHuckel, fromScratch)
{
    DebyeHuckel p;
    p.addUndefinedElements();
    auto sH2O = make_species("H2O(l)", "H:2, O:1", h2oliq_nasa_coeffs);
    auto sNa = make_species("Na+", "Na:1, E:-1", -240.34e6,
                              298.15, -103.98186, 333.15, -103.98186);
    sNa->charge = 1;
    sNa->extra["ionic_radius"] = 4.0e-10;
    auto sCl = make_species("Cl-", "Cl:1, E:1", -167.08e6,
                              298.15, -74.20664, 333.15, -74.20664);
    sCl->charge = -1;
    sCl->extra["ionic_radius"] = 3.0e-10;
    auto sH = make_species("H+", "H:1, E:-1", 0.0, 298.15, 0.0, 333.15, 0.0);
    sH->charge = 1;
    sH->extra["ionic_radius"] = 9.0e-10;
    auto sOH = make_species("OH-", "O:1, H:1, E:1", -230.015e6,
                              298.15, -91.50963, 333.15, -85);
    sOH->charge = -1;
    sOH->extra["ionic_radius"] = 3.5e-10;
    auto sNaCl = make_species("NaCl(aq)", "Na:1, Cl:1", -96.03e6*4.184,
                              298.15, -174.5057463, 333.15, -174.5057463);
    sNaCl->extra["weak_acid_charge"] = -1;
    sNaCl->extra["electrolyte_species_type"] = "weakAcidAssociated";
    for (auto& s : {sH2O, sNa, sCl, sH, sOH, sNaCl}) {
        p.addSpecies(s);
    }
    std::unique_ptr<PDSS_Water> ss(new PDSS_Water());
    p.installPDSS(0, std::move(ss));
    size_t k = 1;
    for (double v : {0.0, 1.3, 1.3, 1.3, 1.3}) {
        std::unique_ptr<PDSS_ConstVol> ss(new PDSS_ConstVol());
        ss->setMolarVolume(v);
        p.installPDSS(k++, std::move(ss));
    }
    p.setDebyeHuckelModel("bdot_with_variable_a");
    p.setA_Debye(1.172576);
    p.setB_Debye(3.2864e9);
    p.setDefaultIonicRadius(3.5e-10);
    p.setMaxIonicStrength(3.0);
    p.useHelgesonFixedForm();
    p.initThermo();
    p.setState_TPM(300, 101325, "Na+:9.3549, Cl-:9.3549, H+:1.0499E-8,"
        "OH-:1.3765E-6,NaCl(aq):0.98492");

    // Regression test based on XML input file
    vector_fp actcoeff(p.nSpecies());
    p.getMolalityActivityCoefficients(actcoeff.data());
    double act_ref[] = {1.21762, 0.538061, 0.472329, 0.717707, 0.507258, 1.0};
    for (size_t k = 0; k < p.nSpecies(); k++) {
        EXPECT_NEAR(actcoeff[k], act_ref[k], 1e-5);
    }
}

TEST(MargulesVPSSTP, fromScratch)
{
    MargulesVPSSTP p;
    p.addUndefinedElements();
    auto sKCl = make_shomate_species("KCl(L)", "K:1 Cl:1", kcl_shomate_coeffs);
    auto sLiCl = make_shomate_species("LiCl(L)", "Li:1 Cl:1", licl_shomate_coeffs);
    p.addSpecies(sKCl);
    p.addSpecies(sLiCl);
    size_t k = 0;
    for (double v : {0.03757, 0.020304}) {
        std::unique_ptr<PDSS_ConstVol> ss(new PDSS_ConstVol());
        ss->setMolarVolume(v);
        p.installPDSS(k++, std::move(ss));
    }
    p.initThermo();
    p.setState_TPX(900, 101325, "KCl(L):0.3, LiCl(L):0.7");
    p.addBinaryInteraction("KCl(L)", "LiCl(L)",
        -1.757e7, -3.77e5, -7.627e3, 4.958e3, 0.0, 0.0, 0.0, 0.0);

    // Regression test based on LiKCl_liquid.xml
    EXPECT_NEAR(p.density(), 2042.1165603245981, 1e-9);
    EXPECT_NEAR(p.gibbs_mass(), -9682981.421693124, 1e-5);
    EXPECT_NEAR(p.cp_mole(), 67478.48085733457, 1e-8);
}

TEST(LatticeSolidPhase, fromScratch)
{
    auto base = make_shared<StoichSubstance>();
    base->addUndefinedElements();
    base->setName("Li7Si3(S)");
    base->setDensity(1390.0);
    auto sLi7Si3 = make_shomate2_species("Li7Si3(S)", "Li:7 Si:3", li7si3_shomate_coeffs);
    base->addSpecies(sLi7Si3);
    base->initThermo();

    auto interstital = make_shared<LatticePhase>();
    interstital->addUndefinedElements();
    interstital->setName("Li7Si3_Interstitial");
    auto sLii = make_const_cp_species("Li(i)", "Li:1", 298.15, 0, 2e4, 2e4);
    auto sVac = make_const_cp_species("V(i)", "", 298.15, 8.98e4, 0, 0);
    sLii->extra["molar_volume"] = 0.2;
    interstital->setSiteDensity(10.46344);
    interstital->addSpecies(sLii);
    interstital->addSpecies(sVac);
    interstital->initThermo();
    interstital->setMoleFractionsByName("Li(i):0.01 V(i):0.99");

    LatticeSolidPhase p;
    p.addUndefinedElements();
    p.addLattice(base);
    p.addLattice(interstital);
    p.setLatticeStoichiometry(parseCompString("Li7Si3(S):1.0 Li7Si3_Interstitial:1.0"));
    p.initThermo();
    p.setState_TP(725, 10 * OneAtm);

    // Regression test based on modified version of Li7Si3_ls.xml
    EXPECT_NEAR(p.enthalpy_mass(), -2077821.9295456698, 1e-6);
    double mu_ref[] = {-4.62717474e+08, -4.64248485e+07, 1.16370186e+05};
    double vol_ref[] = {0.09557086, 0.2, 0.09557086};
    vector_fp mu(p.nSpecies());
    vector_fp vol(p.nSpecies());
    p.getChemPotentials(mu.data());
    p.getPartialMolarVolumes(vol.data());

    for (size_t k = 0; k < p.nSpecies(); k++) {
        EXPECT_NEAR(mu[k], mu_ref[k], 1e-7*fabs(mu_ref[k]));
        EXPECT_NEAR(vol[k], vol_ref[k], 1e-7);
    }
}

TEST(IdealSolidSolnPhase, fromScratch)
{
    // Regression test based fictitious XML input file
    IdealSolidSolnPhase p;
    p.addUndefinedElements();
    auto sp1 = make_species("sp1", "C:2, H:2", o2_nasa_coeffs);
    sp1->extra["molar_volume"] = 1.5;
    auto sp2 = make_species("sp2", "C:1", h2o_nasa_coeffs);
    sp2->extra["molar_volume"] = 1.3;
    auto sp3 = make_species("sp3", "H:2", h2_nasa_coeffs);
    sp3->extra["molar_volume"] = 0.1;
    for (auto& s : {sp1, sp2, sp3}) {
        p.addSpecies(s);
    }
    p.setState_TPX(500, 2e5, "sp1:0.1, sp2:0.89, sp3:0.01");
    EXPECT_NEAR(p.density(), 10.1786978, 1e-6);
    EXPECT_NEAR(p.enthalpy_mass(), -15642803.3884617, 1e-4);
    EXPECT_NEAR(p.gibbs_mole(), -313642293.1654253, 1e-4);
}

} // namespace Cantera
