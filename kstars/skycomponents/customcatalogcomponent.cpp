/***************************************************************************
                          deepskycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/17/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QTextStream>
#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "starobject.h"
#include "deepskyobject.h"

#include "customcatalogcomponent.h"

QStringList CustomCatalogComponent::m_Columns = QString( "ID RA Dc Tp Nm Mg Mj Mn PA Ig" ).split( " ", QString::SkipEmptyParts );

CustomCatalogComponent::CustomCatalogComponent(SkyComponent *parent, const QString &fname, bool showerrs, bool (*visibleMethod)()) : ListComponent(parent, visibleMethod), m_Filename( fname ), m_Showerrs( showerrs )
{
}

CustomCatalogComponent::~CustomCatalogComponent()
{
}

void CustomCatalogComponent::init( KStarsData * ) {
	emitProgressText( i18n("Loading custom catalog: %1", m_Filename ) );

	QDir::setCurrent( QDir::homePath() );  //for files with relative path

	//If the filename begins with "~", replace the "~" with the user's home directory
	//(otherwise, the file will not successfully open)
	if ( m_Filename.at(0)=='~' )
		m_Filename = QDir::homePath() + m_Filename.mid( 1, m_Filename.length() );
	QFile ccFile( m_Filename );

	if ( ccFile.open( QIODevice::ReadOnly ) ) {
		int iStart(0); //the line number of the first non-header line
		QStringList errs; //list of error messages
		QStringList Columns; //list of data column descriptors in the header

		QTextStream stream( &ccFile );
		QStringList lines = stream.readAll().split( "\n", QString::SkipEmptyParts );

		if ( parseCustomDataHeader( lines, Columns, iStart, m_Showerrs, errs ) ) {

			for ( int i=iStart; i < lines.size(); ++i ) {
				QStringList d = lines.at(i).split( " ", QString::SkipEmptyParts );

				//Now, if one of the columns is the "Name" field, the name may contain spaces.
				//In this case, the name field will need to be surrounded by quotes.
				//Check for this, and adjust the d list accordingly
				int iname = Columns.indexOf( "Nm" );
				if ( iname >= 0 && d[iname].left(1) == "\"" ) { //multi-word name in quotes
					d[iname] = d[iname].mid(1); //remove leading quote
					//It's possible that the name is one word, but still in quotes
					if ( d[iname].right(1) == "\"" ) {
						d[iname] = d[iname].left( d[iname].length() - 1 );
					} else {
						int iend = iname + 1;
						while ( d[iend].right(1) != "\"" ) {
							d[iname] += " " + d[iend];
							++iend;
						}
						d[iname] += " " + d[iend].left( d[iend].length() - 1 );

						//remove the entries from d list that were the multiple words in the name
						for ( int j=iname+1; j<=iend; j++ ) {
							d.removeAll( d.at(iname + 1) ); //index is *not* j, because next item becomes "iname+1" after remove
						}
					}
				}

				if ( d.size() == Columns.size() ) {
					processCustomDataLine( i, d, Columns, m_Showerrs, errs );
				} else {
					if ( m_Showerrs ) errs.append( i18n( "Line %1 does not contain %2 fields.  Skipping it.", i, Columns.size() ) );
				}
			}
		}

		if ( objectList().size() ) {
			if ( errs.size() > 0 ) { //some data parsed, but there are errs to report
				QString message( i18n( "Some lines in the custom catalog could not be parsed; see error messages below." ) + "\n" +
												i18n( "To reject the file, press Cancel. " ) +
												i18n( "To accept the file (ignoring unparsed lines), press Accept." ) );
				if ( KMessageBox::warningContinueCancelList( 0, message, errs,
						i18n( "Some Lines in File Were Invalid" ), i18n( "Accept" ) ) != KMessageBox::Continue ) {
					objectList().clear();
					return ;
				}
			}
		} else { //objList.count() == 0
			if ( m_Showerrs ) {
				QString message( i18n( "No lines could be parsed from the specified file, see error messages below." ) );
				KMessageBox::informationList( 0, message, errs,
						i18n( "No Valid Data Found in File" ) );
			}
			objectList().clear();
			return;
		}

	} else { //Error opening catalog file
		if ( m_Showerrs )
			KMessageBox::sorry( 0, i18n( "Could not open custom data file: %1", m_Filename ),
					i18n( "Error opening file" ) );
		else
			kDebug() << i18n( "Could not open custom data file: %1", m_Filename ) << endl;
	}
}

void CustomCatalogComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( ! visible() ) return;

	SkyMap *map = ks->map();
	float Width  = scale * map->width();
	float Height = scale * map->height();

	psky.setBrush( Qt::NoBrush );
	psky.setPen( QColor( m_catColor ) );

	//Draw Custom Catalog objects
	foreach ( SkyObject *obj, objectList() ) {

		if ( map->checkVisibility( obj ) ) {
			QPointF o = map->getXY( obj, Options::useAltAz(), Options::useRefraction(), scale );

			if ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) {

				if ( obj->type()==0 ) {
					StarObject *starobj = (StarObject*)obj;
 					float zoomlim = 7.0 + ( Options::zoomFactor()/MINZOOM)/50.0;
 					float mag = starobj->mag();
 					float sizeFactor = 2.0;
 					int size = int( sizeFactor*(zoomlim - mag) ) + 1;
 					if (size>23) size=23;
					starobj->draw( psky, o.x(), o.y(), size, Options::starColorMode(), Options::starColorIntensity(), true, scale );
				} else {
					//PA for Deep-Sky objects is 90 + PA because major axis is horizontal at PA=0
					DeepSkyObject *dso = (DeepSkyObject*)obj;
					double pa = 90. + map->findPA( dso, o.x(), o.y(), scale );
					dso->drawImage( psky, o.x(), o.y(), pa, Options::zoomFactor() );

					dso->drawSymbol( psky, o.x(), o.y(), pa, Options::zoomFactor() );
				}
			}
		}
	}
}

bool CustomCatalogComponent::parseCustomDataHeader( QStringList lines, QStringList &Columns, int &iStart, bool showerrs, QStringList &errs )
{

	bool foundDataColumns( false ); //set to true if description of data columns found
	int ncol( 0 );

	m_catName = QString();
	m_catPrefix = QString();
	m_catColor = QString();
	m_catEpoch = 0.;
	int i=0;
	for ( ; i < lines.size(); ++i ) {
		QString d( lines.at(i) ); //current data line
		if ( d.left(1) != "#" ) break;  //no longer in header!

		int iname   = d.indexOf( "# Name: " );
		int iprefix = d.indexOf( "# Prefix: " );
		int icolor  = d.indexOf( "# Color: " );
		int iepoch  = d.indexOf( "# Epoch: " );

		if ( iname == 0 ) { //line contains catalog name
			iname = d.indexOf(":")+2;
			if ( m_catName.isEmpty() ) {
				m_catName = d.mid( iname );
			} else { //duplicate name in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) +
							i18n( "Extra Name field in header: %1.  Will be ignored", d.mid(iname) ) );
			}
		} else if ( iprefix == 0 ) { //line contains catalog prefix
			iprefix = d.indexOf(":")+2;
			if ( m_catPrefix.isEmpty() ) {
				m_catPrefix = d.mid( iprefix );
			} else { //duplicate prefix in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) +
							i18n( "Extra Prefix field in header: %1.  Will be ignored", d.mid(iprefix) ) );
			}
		} else if ( icolor == 0 ) { //line contains catalog prefix
			icolor = d.indexOf(":")+2;
			if ( m_catColor.isEmpty() ) {
				m_catColor = d.mid( icolor );
			} else { //duplicate prefix in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) +
							i18n( "Extra Color field in header: %1.  Will be ignored", d.mid(icolor) ) );
			}
		} else if ( iepoch == 0 ) { //line contains catalog epoch
			iepoch = d.indexOf(":")+2;
			if ( m_catEpoch == 0. ) {
				bool ok( false );
				m_catEpoch = d.mid( iepoch ).toFloat( &ok );
				if ( !ok ) {
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) +
								i18n( "Could not convert Epoch to float: %1.  Using 2000. instead", d.mid(iepoch) ) );
					m_catEpoch = 2000.; //adopt default value
				}
			} else { //duplicate epoch in header
				if ( showerrs )
					errs.append( i18n( "Parsing header: " ) +
							i18n( "Extra Epoch field in header: %1.  Will be ignored", d.mid(iepoch) ) );
			}
		} else if ( ! foundDataColumns ) { //don't try to parse data column descriptors if we already found them
			//Chomp off leading "#" character
			d = d.replace( QRegExp( "#" ), QString() );

			QStringList fields = d.split( " ", QString::SkipEmptyParts ); //split on whitespace

			//we need a copy of the master list of data fields, so we can
			//remove fields from it as they are encountered in the "fields" list.
			//this allows us to detect duplicate entries
			QStringList master( m_Columns );

			for ( int j=0; j < fields.size(); ++j ) {
				QString s( fields.at(j) );
				if ( master.contains( s ) ) {
					//add the data field
					Columns.append( s );

					// remove the field from the master list and inc the
					// count of "good" columns (unless field is "Ignore")
					if ( s != "Ig" ) {
						master.removeAt( master.indexOf( s ) );
						ncol++;
					}
				} else if ( fields.contains( s ) ) { //duplicate field
					fields.append( "Ig" ); //ignore the duplicate column
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) +
								i18n( "Duplicate data field descriptor \"%1\" will be ignored", s ) );
				} else { //Invalid field
					fields.append( "Ig" ); //ignore the invalid column
					if ( showerrs )
						errs.append( i18n( "Parsing header: " ) +
								i18n( "Invalid data field descriptor \"%1\" will be ignored", s ) );
				}
			}

			if ( ncol ) foundDataColumns = true;
		}
	}

	if ( ! foundDataColumns ) {
		if ( showerrs )
			errs.append( i18n( "Parsing header: " ) +
					i18n( "No valid column descriptors found.  Exiting" ) );
		return false;
	}

	if ( i == lines.size() ) {
		if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No data lines found after header.  Exiting." ) );
		return false;
	} else {
		//Make sure Name, Prefix, Color and Epoch were set
		if ( m_catName.isEmpty() ) {
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Name specified; setting to \"Custom\"" ) );
			m_catName = i18n("Custom");
		}
		if ( m_catPrefix.isEmpty() ) {
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Prefix specified; setting to \"CC\"" ) );
			m_catPrefix = "CC";
		}
		if ( m_catColor.isEmpty() ) {
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Color specified; setting to Red" ) );
			m_catColor = "#CC0000";
		}
		if ( m_catEpoch == 0. ) {
			if ( showerrs ) errs.append( i18n( "Parsing header: " ) +
				i18n( "No Catalog Epoch specified; assuming 2000." ) );
			m_catEpoch = 2000.;
		}

		//index i now points to the first line past the header
		iStart = i;
		return true;
	}
}

bool CustomCatalogComponent::processCustomDataLine(int lnum, QStringList d, QStringList Columns, bool showerrs, QStringList &errs )
{

	//object data
	unsigned char iType(0);
	dms RA, Dec;
	float mag(0.0), a(0.0), b(0.0), PA(0.0);
	QString name, lname;

	for ( int i=0; i<Columns.size(); i++ ) {
		if ( Columns.at(i) == "ID" )
			name = m_catPrefix + " " + d.at(i);

		if ( Columns.at(i) == "Nm" )
			lname = d.at(i);

		if ( Columns[i] == "RA" ) {
			if ( ! RA.setFromString( d.at(i), false ) ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse RA value: %3" ,
							  lnum, i, d.at(i) ) );
				return false;
			}
		}

		if ( Columns.at(i) == "Dc" ) {
			if ( ! Dec.setFromString( d.at(i), true ) ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Dec value: %3" ,
							  lnum, i, d.at(i) ) );
				return false;
			}
		}

		if ( Columns.at(i) == "Tp" ) {
			bool ok(false);
			iType = d.at(i).toUInt( &ok );
			if ( ok ) {
				if ( iType == 2 || iType > 8 ) {
					if ( showerrs )
						errs.append( i18n( "Line %1, field %2: Invalid object type: %3" ,
								  lnum, i, d.at(i) ) +
								i18n( "Must be one of 0, 1, 3, 4, 5, 6, 7, 8." ) );
					return false;
				}
			} else {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Object type: %3" ,
							  lnum, i, d.at(i) ) );
				return false;
			}
		}

		if ( Columns.at(i) == "Mg" ) {
			bool ok(false);
			mag = d.at(i).toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Magnitude: %3" ,
							  lnum, i, d.at(i) ) );
				return false;
			}
		}

		if ( Columns.at(i) == "Mj" ) {
			bool ok(false);
			a = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Major Axis: %3" ,
							  lnum, i, d.at(i) ) );
				return false;
			}
		}

		if ( Columns.at(i) == "Mn" ) {
			bool ok(false);
			b = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Minor Axis: %3" ,
							  lnum, i, d.at(i) ) );
				return false;
			}
		}

		if ( Columns.at(i) == "PA" ) {
			bool ok(false);
			PA = d[i].toFloat( &ok );
			if ( ! ok ) {
				if ( showerrs )
					errs.append( i18n( "Line %1, field %2: Unable to parse Position Angle: %3" ,
							  lnum, i, d.at(i) ) );
				return false;
			}
		}
	}

	if ( iType == 0 ) { //Add a star
		StarObject *o = new StarObject( RA, Dec, mag, lname );
		objectList().append( o );
	} else { //Add a deep-sky object
		DeepSkyObject *o = new DeepSkyObject( iType, RA, Dec, mag,
					name, QString(), lname, m_catPrefix, a, b, PA );
		objectList().append( o );

		//Add name to the list of object names
		if ( ! name.isEmpty() )
			objectNames().append( name );
	}
	if ( ! lname.isEmpty() && lname != name )
		objectNames().append( lname );

	return true;
}
