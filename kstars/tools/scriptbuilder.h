/***************************************************************************
                          scriptbuilder.h  -  description
                             -------------------
    begin                : Thu Apr 17 2003
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

#ifndef SCRIPTBUILDER_H_
#define SCRIPTBUILDER_H_

#include <kdialog.h>

#include "ui_scriptbuilder.h"
#include "ui_scriptnamedialog.h"
#include "ui_optionstreeview.h"
#include "scriptargwidgets.h"

class QTreeWidget;
class QTextStream;
class QVBoxLayout;
class KUrl;

class KStars;
class ScriptFunction;

class OptionsTreeViewWidget : public QFrame, public Ui::OptionsTreeView {
Q_OBJECT
public:
	OptionsTreeViewWidget( QWidget *p );
};

class OptionsTreeView : public KDialog {
Q_OBJECT
public:
	OptionsTreeView( QWidget *p );
	~OptionsTreeView();
	QTreeWidget* optionsList() { return otvw->OptionsList; }
 
private:
	OptionsTreeViewWidget *otvw;
};

class ScriptNameWidget : public QFrame, public Ui::ScriptNameDialog {
Q_OBJECT
public:
	ScriptNameWidget( QWidget *p );
};

class ScriptNameDialog : public KDialog {
Q_OBJECT
public:
	ScriptNameDialog( QWidget *p );
	~ScriptNameDialog();
	QString scriptName() const { return snw->ScriptName->text(); }
	QString authorName() const { return snw->AuthorName->text(); }
 
private slots: 
	void slotEnableOkButton();

private:
	ScriptNameWidget *snw;
};

class ScriptBuilderUI : public QFrame, public Ui::ScriptBuilder {
Q_OBJECT
public:
	ScriptBuilderUI( QWidget *p );
};

/**@class ScriptBuilder
	*A GUI tool for building behavioral DCOP scripts for KStars.
	*@author Jason Harris
	*@version 1.0
	*/
class ScriptBuilder : public KDialog {
Q_OBJECT
public:
	ScriptBuilder( QWidget *parent );
	~ScriptBuilder();

	bool unsavedChanges() const { return UnsavedChanges; }
	void setUnsavedChanges( bool b=true );
	void saveWarning();
	void readScript( QTextStream &istream );
	void writeScript( QTextStream &ostream );
	bool parseFunction( QStringList &fn );

public slots:
	void slotAddFunction();
	void slotMoveFunctionUp();
	void slotMoveFunctionDown();
	void slotArgWidget();
	void slotShowDoc();

	void slotNew();
	void slotOpen();
	void slotSave();
	void slotSaveAs();
	void slotRunScript();
	void slotClose();

	void slotCopyFunction();
	void slotRemoveFunction();

	void slotFindCity();
	void slotFindObject();
	void slotShowOptions();
	void slotLookToward();
	void slotRa();
	void slotDec();
	void slotAz();
	void slotAlt();
	void slotChangeDate();
	void slotChangeTime();
	void slotWaitFor();
	void slotWaitForKey();
	void slotTracking();
	void slotViewOption();
	void slotChangeCity();
	void slotChangeProvince();
	void slotChangeCountry();
	void slotTimeScale();
	void slotZoom();
	void slotExportImage();
	void slotPrintImage();
	void slotChangeColor();
	void slotChangeColorName();
	void slotLoadColorScheme();
	
	void slotINDIWaitCheck(bool toggleState);
	void slotINDIFindObject();
	void slotINDIStartDeviceName();
	void slotINDIStartDeviceMode();
	void slotINDISetDevice();
	void slotINDIShutdown();
	void slotINDISwitchDeviceConnection();
	void slotINDISetPortDevicePort();
	void slotINDISetTargetCoordDeviceRA();
	void slotINDISetTargetCoordDeviceDEC();
	void slotINDISetTargetNameTargetName();
	void slotINDISetActionName();
	void slotINDIWaitForActionName();
	void slotINDISetFocusSpeed();
	void slotINDIStartFocusDirection();
	void slotINDISetFocusTimeout();
	void slotINDISetGeoLocationDeviceLong();
	void slotINDISetGeoLocationDeviceLat();
	void slotINDIStartExposureTimeout();
	void slotINDISetUTC();
	void slotINDISetScopeAction();
	void slotINDISetFrameType();
	void slotINDISetCCDTemp();
	void slotINDISetFilterNum();

private:
	void initViewOptions();
	void warningMismatch (const QString &expected) const;

	ScriptBuilderUI *sb;

	KStars *ks; //parent needed for sub-dialogs
	QList<ScriptFunction*> KStarsFunctionList;
	QList<ScriptFunction*> INDIFunctionList;
	QList<ScriptFunction*> ScriptList;
	QVBoxLayout *vlay;

	QWidget *argBlank;
	ArgLookToward *argLookToward;
	ArgSetRaDec *argSetRaDec;
	ArgSetAltAz *argSetAltAz;
	ArgSetLocalTime *argSetLocalTime;
	ArgWaitFor *argWaitFor;
	ArgWaitForKey *argWaitForKey;
	ArgSetTrack *argSetTracking;
	ArgChangeViewOption *argChangeViewOption;
	ArgSetGeoLocation *argSetGeoLocation;
	ArgTimeScale *argTimeScale;
	ArgZoom *argZoom;
	ArgExportImage *argExportImage;
	ArgPrintImage *argPrintImage;
	ArgSetColor *argSetColor;
	ArgLoadColorScheme *argLoadColorScheme;
	ArgStartINDI *argStartINDI;
	ArgSetDeviceINDI *argSetDeviceINDI;
	ArgShutdownINDI *argShutdownINDI;
	ArgSwitchINDI *argSwitchINDI;
	ArgSetPortINDI *argSetPortINDI;
	ArgSetTargetCoordINDI *argSetTargetCoordINDI;
	ArgSetTargetNameINDI *argSetTargetNameINDI;
	ArgSetActionINDI *argSetActionINDI;
	ArgSetActionINDI *argWaitForActionINDI;
	ArgSetFocusSpeedINDI *argSetFocusSpeedINDI;
	ArgStartFocusINDI *argStartFocusINDI;
	ArgSetFocusTimeoutINDI *argSetFocusTimeoutINDI;
	ArgSetGeoLocationINDI *argSetGeoLocationINDI;
	ArgStartExposureINDI *argStartExposureINDI;
	ArgSetUTCINDI *argSetUTCINDI;
	ArgSetScopeActionINDI *argSetScopeActionINDI;
	ArgSetFrameTypeINDI *argSetFrameTypeINDI;
	ArgSetCCDTempINDI *argSetCCDTempINDI;
	ArgSetFilterNumINDI *argSetFilterNumINDI;
	
	ScriptNameDialog *snd;
	OptionsTreeView *otv;

	QTreeWidgetItem *opsGUI, *opsToolbar, *opsShowObj, *opsShowOther, *opsCName, *opsHide, *opsSkymap, *opsLimit;

	bool UnsavedChanges;
	KUrl currentFileURL;
	QString currentDir;
	QString currentScriptName, currentAuthor;
};

#endif
