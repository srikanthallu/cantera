/**
 *
 *  @file State.h
 *
 *  This file implements class State.
 */

/*
 *  $Author$
 *  $Date$
 *  $Revision$
 *
 *  Copyright 2001-2003 California Institute of Technology
 *  See file License.txt for licensing information
 *
 */


#ifndef CT_STATE2_H
#define CT_STATE2_H

#include "utilities.h"
#include "ctexceptions.h"

namespace Cantera {

    /**
     * Manages the thermodynamic state. Class State manages the
     * thermodynamic state of a multi-species solution. It holds
     * values for the temperature, mass density, and mean molecular
     * weight, and a vector of species mass fractions. For efficiency
     * in mass/mole conversion, the vector of mass fractions divided
     * by molecular weight \f$ Y_k/M_k \f$ is also stored.
     *
     * Class State is not usually used directly in application
     * programs. Its primary use is as a base class for class Phase.
     */

    class State {

    public:

	/**
         * Constructor. 
         */
	State();
            
	/**
         * Destructor. Since no memory is allocated by methods of this
         * class, the destructor does nothing.
         */
	virtual ~State();

        /**
         * Return a read-only reference to the array of molecular
         * weights.
         */
        const array_fp& molecularWeights() const { return m_molwts; }

	/**
         * Get the species mole fractions.
         * @param x On return, x contains the mole fractions. Must have a
         * length greater than or equal to the number of species.
         */
	void getMoleFractions(doublereal* x) const {
            scale(m_ym.begin(), m_ym.end(), x, m_mmw);
        }

        /// The mole fraction of species k.
        doublereal moleFraction(int k) const;

	/**
         * Set the mole fractions to the specified values, and then 
         * normalize them so that they sum to 1.0.
         * @param x Array of unnormalized mole fraction values (input). 
         * Must have a length greater than or equal to the number of
         * species.
         */
	void setMoleFractions(const doublereal* x);

	/**
         * Set the mole fractions to the specified values without
         * normalizing.
         */
	void setMoleFractions_NoNorm(const doublereal* x);

        /**
         * Get the species mass fractions. 
         * @param y On return, y contains the mass fractions. Array y 
         * must have a length at least as large as the number of species.
         */
	void getMassFractions(size_t leny, doublereal* y) const {
            copy(m_y.begin(), m_y.end(), y);
        }

        /**
         * Get the species mass fractions.  @param y On return, y
         * contains the mass fractions. Array \i y must have a length
         * greater than or equal to the number of species.
         */
	void getMassFractions(doublereal* y) const {
            copy(m_y.begin(), m_y.end(), y);
        }

        /// Mass fraction of species k.
        doublereal massFraction(int k) const;


	/**
         * Set the mole fractions to the specified values, and then 
         * normalize them so that they sum to 1.0.
         * @param x Array of unnormalized mole fraction values (input). 
         * Must have a length greater than or equal to the number of
         * species.
         */
	void setMassFractions(const doublereal* y);

	/**
         * Set the mass fractions to the specified values without
         * normalizing.
         */
	void setMassFractions_NoNorm(const doublereal* y);

        /**
         * Get the species concentrations (kmol/m^3).  
         * @param c On return, \i c contains the concentrations. 
         * Array \i c must have a length greater than or equal to 
         * the number of species.
         */
	void getConcentrations(doublereal* c) const {
            doublereal f = m_dens;
            scale(m_ym.begin(), m_ym.end(), c, f);
        }

	/**
         * Evaluate the mole-fraction-weighted mean of Q:
         * \f[ \sum_k X_k Q_k. \f]
         * Array Q should contain pure-species molar property 
         * values.
         */
	doublereal mean_X(const doublereal* Q) const {
            return m_mmw*dot(m_ym.begin(), m_ym.end(), Q);
        }


	/**
         * Evaluate the mass-fraction-weighted mean of Q:
         * \f[ \sum_k Y_k Q_k \f]
         * Array Q should contain pure-species property 
         * values in mass units.
         */        
	doublereal mean_Y(const doublereal* Q) const {
            return dot(m_y.begin(), m_y.end(), Q);
	}

	/**
         * The mean molecular weight. Units: (kg/kmol)
         */
	doublereal meanMolecularWeight() const {
            return m_mmw; 
	}

	/// Evaluate \f$ \sum_k X_k \log X_k \f$.
	doublereal sum_xlogx() const;

	/// Evaluate \f$ \sum_k X_k \log Q_k \f$.
	doublereal sum_xlogQ(doublereal* Q) const;

	/// Temperature. Units: K.
	doublereal temperature() const { return m_temp; }

	/// Density. Units: kg/m^3.
	doublereal density() const { return m_dens; }

	/// Molar density. Units: kmol/m^3.
	doublereal molarDensity() const { 
            return m_dens/meanMolecularWeight(); 
        }

	/// Set the density to value rho (kg/m^3).
	void setDensity(doublereal rho) {
            if (rho != m_dens) {
                m_dens = rho;
                //m_C_updater.need_update();
            }
        }

	/// Set the molar density to value n (kmol/m^3).
	void setMolarDensity(doublereal n) {
            m_dens = n*meanMolecularWeight();
            //m_C_updater.need_update();
        }

	/// Set the temperature to value temp (K).
	void setTemperature(doublereal temp) {
            //if (temp != m_temp) {
                m_temp = temp;
                //m_T_updater.need_update();
                //} 
        }

	/**
	 * Set the concentrations to the specified values within the
	 * phase. This is the MAIN function for internally setting
	 * the composition of a phase. It sets all of the internal
	 * parameters within the state object except the temperature. 
         * These are:
	 *    m_dens = density of state
	 *    m_ym[k] = mole fraction of species k / MolecWeight species k
	 *    m_y[k] = Mass fractions of species k
	 *    m_mmw = mean molecular weight of mixture
	 *
	 * @param The input vector to this routine is in dimensional
	 *        units. For volumetric phases c[k] is the
	 *        concentration of the kth species in kmol/m3.
	 *        For surface phases, c[k] is the concentration
	 *        in kmol/m2. The length of the vector is the number
	 *        of species in the phase.
	 */
	void setConcentrations(const doublereal* c);

	/**
	 * Returns a pointer to the start of the massFraction array
	 */
        const doublereal* massFractions() const { return m_y.begin(); }

	/**
	 * Returns a pointer to the start of the moleFraction/MW array.
	 * This array is the array of mole fractions, each divided by
	 * the mean molecular weight.
	 */
	const doublereal* moleFractdivMMW() const { return m_ym.begin();}

        bool ready() const { return (m_kk > 0); }


    protected:

        /**
         * @internal 
         * Initialize. Make a local copy of the vector of
         * molecular weights, and resize the composition arrays to
         * the appropriate size. The only information an instance of
         * State has about the species is their molecular weights. 
         *
	 */
 	void init(const array_fp& mw); //, density_is_independent = true);

	/**
	 * m_kk is the number of species in the mixture
	 */
        int m_kk;

    private:

	/**
	 * Temperature. This is an independent variable 
	 * units = Kelvin
	 */
        doublereal m_temp;

	/**
	 * Density -> this is an independent variable except in
	 *            the incompressible degenerate case. Thus,
	 *            the pressure is determined from this variable
	 *            not the other way round.
	 * units = kg m-3
	 */
	doublereal m_dens;

	/**
	 * m_mmw is the mean molecular weight of the mixture
	 * (kg kmol-1)
	 */

        doublereal m_mmw;
        
	/**
	 *  m_ym[k] = mole fraction of species k divided by the
	 *            mean molecular weight of mixture.
	 */
        mutable array_fp m_ym;

	/**
	 * m_y[k]  = mass fraction of species k
	 */
        mutable array_fp m_y;

	/**
	 * m_molwts[k] = molecular weight of species k (kg kmol-1)
	 */
	array_fp m_molwts;

	/**
	 *  m_rmolwts[k] = inverse of the molecular weight of species k
	 *  units = kmol kg-1.
	 */
	array_fp m_rmolwts;

    };

}

#endif













