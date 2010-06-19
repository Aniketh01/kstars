/*
    SkyPainter: class for painting onto the sky for KStars
    Copyright (C) 2010 Henry de Valence <hdevalence@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#ifndef SKYPAINTER_H
#define SKYPAINTER_H

#include <QPainter>

#include "skycomponents/typedef.h"

class TrailObject;
class KSPlanetBase;
class KSAsteroid;
class KSComet;
class DeepSkyObject;
class SkyPoint;
class SkyMap;
class SkipList;
class LineList;
class LineListLabel;


/** @short Draws things on the sky, without regard to backend.
    This class serves as an interface to draw objects onto the sky without
    worrying about whether we are using a QPainter or OpenGL.
. */
class SkyPainter
{
public:
    /** @short Constructor.
        @param sm A pointer to SkyMap object on which to paint.
        */
    SkyPainter(SkyMap *sm);
    virtual ~SkyPainter();
    
    /** @short Get the SkyMap on which the painter operates */
    SkyMap* skyMap() const;

    /** @short Set the pen of the painter **/
    virtual void setPen(const QPen& pen) = 0;

    /** @short Set the brush of the painter **/
    virtual void setBrush(const QBrush& brush) = 0;

    //FIXME: find a better way to do this.
    void setSizeMagLimit(float sizeMagLim);

    ////////////////////////////////////
    //                                //
    // SKY DRAWING FUNCTIONS:         //
    //                                //
    ////////////////////////////////////

    /** @short Draw a line between points in the sky.
        @param a the first point
        @param b the second point
        @note this function will skip lines not on screen and clip lines
               that are only partially visible. */
    virtual void drawSkyLine(SkyPoint* a, SkyPoint* b) =0;

    /** @short Draw a polyline in the sky.
        @param list a list of points in the sky
        @param skipList a SkipList object used to control skipping line segments
        @param label a pointer to the label for this line
        @note it's more efficient to use this than repeated calls to drawSkyLine(),
               because it avoids an extra points->size() -2 projections.
        */
    virtual void drawSkyPolyline(LineList* list, SkipList *skipList = 0,
                                 LineListLabel *label = 0) =0;
    
    /** @short Draw a polygon in the sky.
        @param list a list of points in the sky
        @see drawSkyPolyline()
        */
    virtual void drawSkyPolygon(LineList* list) =0;

    /** @short Draw a point source (e.g., a star).
        @param loc the location of the source in the sky
        @param mag the magnitude of the source
        @param sp the spectral class of the source
        @return true if a source was drawn
        */
    virtual bool drawPointSource(SkyPoint *loc, float mag, char sp = 'A') =0;

    /** @short Draw a deep sky object
        @param obj the object to draw
        @param drawImage if true, try to draw the image of the object
        @return true if it was drawn
        */
    virtual bool drawDeepSkyObject(DeepSkyObject *obj, bool drawImage = false) =0;

    /** @short Draw a comet
        @param comet the comet to draw
        @return true if it was drawn
        */
    virtual bool drawComet(KSComet *comet) =0;

    /** @short Draw an asteroid
        @param ast the asteroid to draw
        @return true if it was drawn
        */
    virtual bool drawAsteroid(KSAsteroid *ast) =0;

    /** @short Draw a planet
        @param planet the planet to draw
        @return true if it was drawn
        */
    virtual bool drawPlanet(KSPlanetBase *planet) =0;

    /** @short Draw a moon
        @param moon the moon to draw
        @return true if it was drawn
        */
    virtual bool drawPlanetMoon(TrailObject *moon) =0;

protected:

    /** @short Get the width of a star of magnitude mag */
    float starWidth(float mag) const;

    SkyMap *m_sm;

private:
    float m_sizeMagLim;
};

#endif // SKYPAINTER_H
