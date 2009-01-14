/***************************************************************************
                          kscomet.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 19 Feb 2003
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSCOMET_H_
#define KSCOMET_H_

#include "ksplanetbase.h"

/**@class KSComet
	*@short A subclass of KSPlanetBase that implements comets.
	*
	*The orbital elements are stored as private member variables, and
	*it provides methods to compute the ecliptic coordinates for any
	*time from the orbital elements.
	*
	*The orbital elements are:
	*@li JD    Epoch of element values
	*@li q     perihelion distance (AU)
	*@li e     eccentricity of orbit
	*@li i     inclination angle (with respect to J2000.0 ecliptic plane)
	*@li w     argument of perihelion (w.r.t. J2000.0 ecliptic plane)
	*@li N     longitude of ascending node (J2000.0 ecliptic)
	*@li Tp    time of perihelion passage (YYYYMMDD.DDD)
        *@li H     absolute magnitude
        *@li G     slope parameter
	*
	*@author Jason Harris
	*@version 1.1
	*/

class KStarsData;
class KSNumbers;
class dms;

class KSComet : public KSPlanetBase
{
public:
    /**Constructor.
    	*@param kd pointer to the KStarsData object
    	*@param s the name of the comet
    	*@param image_file the filename for an image of the comet
    	*@param JD the Julian Day for the orbital elements
    	*@param q the perihelion distance of the comet's orbit (AU)
    	*@param e the eccentricity of the comet's orbit
    	*@param i the inclination angle of the comet's orbit
    	*@param w the argument of the orbit's perihelion
    	*@param N the longitude of the orbit's ascending node
    	*@param Tp The date of the most proximate perihelion passage (YYYYMMDD.DDD)
        *@param H the absolute magnitude
        *@param G the slope parameter
    	*/
    KSComet( KStarsData *kd, const QString &s, const QString &image_file,
             long double JD, double q, double e, dms i, dms w, dms N, double Tp, float H, float G );
    
    /**
     *Copy Constructor
     *@param o  Object to copy into this
     */
    KSComet( KSComet &o );

    /**Destructor (empty)*/
    virtual ~KSComet() {}

    /**Unused virtual function inherited from KSPlanetBase,
    	*so it's simply empty here.
    	*/
    virtual bool loadData();

    /**
     *@short Loads the orbital elements into the given pointers
     *
     *@param _JD  Julian Day of Orbital Elements
     *@param _q the perihelion distance of the comet's orbit (AU)
     *@param _e the eccentricity of the comet's orbit
     *@param _i the inclination angle of the comet's orbit
     *@param _w the argument of the orbit's perihelion
     *@param _N the longitude of the orbit's ascending node
     *
     *@return true on success, false if one or more pointers were NULL
     */
    bool getOrbitalElements( long double *_JD, double *_q, double *_e, dms *_i,
                             dms *_w, dms *_N );

    /**
     *@short Returns the Julian Day of Perihelion passage
     *@return Julian Day of Perihelion Passage
     */
    inline long double getPerihelionJD() { return JDp; }

    /**
     *@return the slope parameter
     */
    inline float getSlopeParameter() { return G; }

    /**
     *@return the absolute magnitude
     */
    inline float getAbsoluteMagnitude() { return H; }


protected:
    /**Calculate the geocentric RA, Dec coordinates of the Comet.
    	*@note reimplemented from KSPlanetBase
    	*@param num time-dependent values for the desired date
    	*@param Earth planet Earth (needed to calculate geocentric coords)
    	*@return true if position was successfully calculated.
    	*/
    virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth=NULL );

private:
    KStarsData *kd;
    long double JD, JDp;
    double q, e, a, P;
    float H, G;
    dms i, w, N;

};

#endif
