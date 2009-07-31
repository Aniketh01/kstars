/***************************************************************************
                          fov.h  -  description
                             -------------------
    begin                : Fri 05 Sept 2003
    copyright            : (C) 2003 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FOV_H_
#define FOV_H_

#include <QList>

#include <qstring.h>
#include <klocale.h>

class QPainter;

/**@class FOV A simple class encapulating a Field-of-View symbol
	*@author Jason Harris
	*@version 1.0
	*/
class FOV {
public:
    enum Shape { SQUARE,
                 CIRCLE,
                 CROSSHAIRS,
                 BULLSEYE,
                 SOLIDCIRCLE,
                 UNKNOWN };
    static FOV::Shape intToShape(int); 
    
    /**Default constructor*/
    FOV();
    FOV( const QString &name );  //in this case, read params from fov.dat
    FOV( const QString &name, float a, float b=-1, Shape shape=SQUARE, const QString &color="#FFFFFF" );
    ~FOV() {}

    inline QString name() const { return m_name; }
    void setName( const QString &n ) { m_name = n; }

    inline Shape shape() const { return m_shape; }
    void setShape( Shape s ) { m_shape = s; }
    void setShape( int s);
    
    inline float sizeX() const { return m_sizeX; }
    inline float sizeY() const { return m_sizeY; }
    void setSize( float s ) { m_sizeX = m_sizeY = s; }
    void setSize( float sx, float sy ) { m_sizeX = sx; m_sizeY = sy; }

    inline QString color() const { return m_color; }
    void setColor( const QString &c ) { m_color = c; }

    /**@short draw the FOV symbol on a QPainter
    	*@param p reference to the target QPainter.  The painter should already be started.
    	*@param size the size of the target symbol, in pixels.
    	*/
    void draw( QPainter &p, float zoomFactor);

    /** @short Fill list with default FOVs*/
    static QList<FOV*> defaults();
    /** @short Write list of FOVs to "fov.dat" */
    static void writeFOVs(const QList<FOV*> fovs);
    /** @short Read list of FOVs from "fov.dat" */
    static QList<FOV*>readFOVs();
private:
    QString m_name,  m_color;
    float   m_sizeX, m_sizeY;
    Shape   m_shape;
};

#endif
