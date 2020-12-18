
/*********************************************************************************************

    This is public domain software that was developed by or for the U.S. Naval Oceanographic
    Office and/or the U.S. Army Corps of Engineers.

    This is a work of the U.S. Government. In accordance with 17 USC 105, copyright protection
    is not available for any work of the U.S. Government.

    Neither the United States Government nor any employees of the United States Government,
    makes any warranty, express or implied, without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, or assumes any liability or
    responsibility for the accuracy, completeness, or usefulness of any information,
    apparatus, product, or process disclosed, or represents that its use would not infringe
    privately-owned rights. Reference herein to any specific commercial products, process,
    or service by trade name, trademark, manufacturer, or otherwise, does not necessarily
    constitute or imply its endorsement, recommendation, or favoring by the United States
    Government. The views and opinions of authors expressed herein do not necessarily state
    or reflect those of the United States Government, and shall not be used for advertising
    or product endorsement purposes.

*********************************************************************************************/

/*  CZMILwaveMonitor class definitions.  */

#ifndef CZMILwaveMonitor_H
#define CZMILwaveMonitor_H

using namespace std;

#include <stdio.h>
#include <fstream>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <getopt.h>
#include <cmath>

// included for file open and save

#include <QFileDialog>

#include "nvutility.h"
#include "pfm_nvtypes.h"
#include "pfm.h"
#include "fixpos.h"
#include "pfm_extras.h"
#include "nvmap.hpp"

#include "ABE.h"
#include "version.hpp"

#include "czmil.h"

#ifdef NVWIN3X
    #include "windows_getuid.h"
#endif

#include <QtCore>
#include <QtGui>
#if QT_VERSION >= 0x050000
#include <QtWidgets>
#endif


// axis bounds (time/count)

#define LOWER_TIME_BOUND	0
#define	UPPER_TIME_BOUND	640

#define LOWER_COUNT_BOUND	0
#define	UPPER_COUNT_BOUND	1023

#define WAVE_X_SIZE             600
#define WAVE_Y_SIZE             270
#define SPOT_SIZE               2

// possible definitions for color codes

#define CC_BY_CURSOR		0
#define CC_BY_FLIGHTLINE	1

#define NUMSHADES		100

#define NUM_WAVEFORMS		10

#define WAVE_ALPHA              64                 // alpha value applied to some colors for the waveforms
#define LBL_PIX_SPACING         5                  // arbitrary buffer space for legend between pane and axis
#define MAX_HISTORY_ENTRIES     100                // number of maximum entries for hash table


#define  NMAX                   2048               //  Maximum number of input data ordinates
#define  NP                     50                 //  Maximum number of filter coefficients


typedef struct
{
  int32_t             min_x;
  int32_t             max_x;
  int32_t             min_y;
  int32_t             max_y;
  int32_t             range_x;
  int32_t             range_y;
  int32_t             length;
} BOUNDS;


struct SavedSettings
{
  uint8_t visible[NUM_WAVEFORMS];		
  QString saveDirectory, openDirectory;
};


class CZMILwaveMonitor:public QMainWindow
{
  Q_OBJECT 


public:

  CZMILwaveMonitor (int32_t *argc = 0, char **argv = 0, QWidget *parent = 0);
  ~CZMILwaveMonitor ();


protected:
  
  int32_t             key, pfm_handle[MAX_ABE_PFMS], num_pfms;
  uint32_t            kill_switch;

  uint8_t             current_channel_mode;
  QSharedMemory       *abeShare;        
  PFM_OPEN_ARGS       open_args[MAX_ABE_PFMS];   
  
  BOUNDS              chanBounds[NUM_WAVEFORMS];
  
  int32_t             width, height, window_x, window_y;
 
  char                cwf_path[512], filename[512], program_version[256], progname[256];
  int32_t             flightline, recnum, chan, ret, current_record;

  ABE_SHARE           l_share;

  CZMIL_CPF_Data      cpf_record;
  
  uint8_t             wave_line_mode, startup_message, interest_point_mode;   

  uint8_t             showIntMeter;        
  int32_t             sigBegin;                 
  int32_t             sigEnd;  
  
  // we made the binHighlighted and scaleBinHiglighted class variables arrays corresponding with
  // the sensor panes.  axisPixel* arrays indicate pixel positions of the beginning and end of the
  // time axis for the intensity meter marker.

  int32_t             axisPixelBegin[NUM_WAVEFORMS];
  int32_t             axisPixelEnd[NUM_WAVEFORMS];  

  int32_t             binHighlighted[NUM_WAVEFORMS];
  int32_t             scaledBinHighlighted[NUM_WAVEFORMS];    

  // we need a variable to hold the current color coding mode

  int32_t             ccMode;    
  
  QString             binTxtLbl;  
  QString             chanTxtLbl[NUM_WAVEFORMS];
  
  int32_t             pt12Height, pt8Height, verLblSpace, pt12Width, gridWidth, horLblSpace, timeWidth, countsWidth;

  // need a width for potential reference file label as well as the label itself

  QString             refFileLbl;
  int32_t             refFileWidth;   
  
  int32_t             active_window_id;
  QMenu               *contextMenu;
  uint8_t             zoomFlag;
  uint8_t             zoomIsActive;
  nvMap               *zoomMap;
  int32_t             zoom_rectangle;
  
  CZMIL_CWF_Data      cwf_record;
    
  int32_t             cwf_read;		
  
  uint16_t            ***referenceData;  
  
  uint8_t             drawReference;
  int32_t             numReferences;	 

  QAction             *deepChanAct, *shal1ChanAct, *shal2ChanAct, *shal3ChanAct, *shal4ChanAct, *shal5ChanAct, *shal6ChanAct, *shal7ChanAct, *irChanAct, *t0ChanAct;

  QColor              referenceColor;

  QFont               font;

  QMessageBox         *filError;

  QCheckBox           *sMessage;

  QStatusBar          *statusBar;

  QColor              surfaceColor, primaryColor, secondaryColor, highlightColor, backgroundColor;

  QPalette            bSurfacePalette, bPrimaryPalette, bSecondaryPalette, bHighlightPalette, bBackgroundPalette;

  QPushButton         *bSurfaceColor, *bPrimaryColor, *bSecondaryColor, *bHighlightColor, *bBackgroundColor, *bRestoreDefaults; 

  // adding a button and palette for the Prefs dialog for the reference mark color

  QPushButton         *bReferenceColor;
  QPalette            bReferencePalette;  

  QAction             *nearAct, *noneAct;
  
  uint8_t             force_redraw, lock_track;
  
  uint8_t             nonFileIO_force_redraw;
  
  nvMap               *map[NUM_WAVEFORMS];
  
  QColor              waveColor;

  NVMAP_DEF           mapdef;

  uint32_t            ac[13];

  QButtonGroup        *bGrp;

  QDialog             *prefsD;

  QToolButton         *bQuit, *bPrefs, *bMode;

  QLabel              *nameLabel, *recordLabel, *simDepth;

  QString             sim_depth;

  QString             db_name, date_time;

  QToolButton         *bIntMeter;

  ABE_SHARE           *abe_share, l_abe_share;

  int32_t             cwf_read_multi[MAX_STACK_POINTS];
  
  char                cpf_path_multi[MAX_STACK_POINTS][512];
   
  char                line_name_multi[MAX_STACK_POINTS][128];

  char                cwf_path_multi[MAX_STACK_POINTS][512];  
  
  int32_t             recnum_multi[MAX_STACK_POINTS], chan_multi[MAX_STACK_POINTS], ret_multi[MAX_STACK_POINTS];
  
  CZMIL_CPF_Data      cpf_record_multi[MAX_STACK_POINTS]; 
    
  CZMIL_CWF_Data      cwf_record_multi[MAX_STACK_POINTS]; 

  QColor              waveColorNear, primaryColorNear, secondaryColorNear, transWave2ColorNear, transPrimaryColorNear, transWaveColorNear, transSecondaryColorNear,
                      wave2ColorNear;

  int32_t             multiWaveMode;
  
  int32_t             lTimeBound[NUM_WAVEFORMS], uTimeBound[NUM_WAVEFORMS], lCountBound[NUM_WAVEFORMS], uCountBound[NUM_WAVEFORMS];

  // array for sensor maps denoting them being blocked or not
  
  uint8_t             mapBlocked[NUM_WAVEFORMS];
    
  // color array

  QColor              colorArray[NUMSHADES * 2];

  // Right now, SavedSettings is a structure containing the pane visibilities of the sensors

  SavedSettings       savedSettings;  

  QString             pos_format, parentName;


  // action items that need to be class variables because we will be enabling/disabling based
  // on user interaction.

  QAction             *toggleReference, *clearReference; 

  QCursor             zoomCursor;

  float               signal[NMAX + 1], ysave[NMAX + 1];
  float               coeff[NP + 1];
  int32_t             ndex[NP + 1], nleft, nright, moment, ndata;


  void envin ();
  void envout ();

  void leftMouse (int32_t x, int32_t y, nvMap * map);
  void midMouse (double x, double y);

  void rightMouse (int32_t x, int32_t y, nvMap * l_map);

  void scaleWave (int32_t x, int32_t y, int32_t *pix_x, int32_t *pix_y, int32_t type, NVMAP_DEF l_mapdef);
  void unScaleWave (int32_t pix_x, int32_t pix_y, int32_t *x, int32_t *y, int32_t type, NVMAP_DEF l_mapdef);
  void closeEvent (QCloseEvent *);

  void drawX (int32_t x, int32_t y, int32_t size, int32_t width, QColor color, nvMap *);
	
  // separated drawing code out slotplotwave

  void drawSingleWave(nvMap *l_map, NVMAP_DEF &l_mapdef, uint16_t * dat, int32_t mapIndex, int32_t mapLength, QColor waveColor );
						
  void drawSingleWave(nvMap *l_map, NVMAP_DEF &l_mapdef, CZMIL_CWF_Data * wData, int32_t mapIndex, int32_t mapLength, QColor waveColor );
						
  void getWaveformData (int32_t mapIndex, uint16_t * &data, uint8_t * &ndx, CZMIL_CWF_Data * wData);

  uint8_t trimWaveformNdxs (uint8_t * ndx, int32_t &start, int32_t &end);
    
  void setFields ();
  
  void writeWaveformHeader(ofstream &outFile, QString &fileName);
  void writeWaveformData (ofstream &outFile);
  void writeSingleTableEntry (ofstream &outfile, int32_t timeIndex);
  void writeNNTableEntry (ofstream &outfile, int32_t timeIndex);

  void loadReferenceData(QTextStream &fileDat, int32_t numCols);
  void allocateReferenceData(int32_t numWaves);
  void clearReferenceData();


protected slots:

  void slotMousePress (QMouseEvent *e, double x, double y);
  void slotMouseMove (QMouseEvent *e, double x, double y);

  void slotResize (QResizeEvent *e);
  
  // This slot catches the pane display toggles of the menu via a lone signal mapper

  void slotPaneToggle (QAction *); 
  
  // slots for the color coding menu options

  void slotColorByCursor (void);
  void slotColorByFlightline (void);  

  void slotPlotWaves (NVMAP_DEF l_mapdef);

  void trackCursor ();

  void slotHelp ();

  void slotQuit (); 

  void slotChannel ();
  void slotMode ();
  void slotInterest ();
  void slotIntMeter ();
  void slotNear ();
  void slotNone ();
  void slotZoomIn ();
  void slotResetZoom ();
	
  // slot to be call from right contextMenu's resetZoom Action
  // different from slotResetZoom in that this function only
  // reset zoom for map over whick right click occurs

  void slotContextResetZoom (); 

  void slotPrefs ();
  void slotPosClicked (int id);
  void slotClosePrefs ();
	
  // Added slot for opening and saveing waveform files
	
  void slotOpen ();
  void slotSave ();	

  // slots for the Reference marks menu options and color button in Prefs dialog

  void slotToggleReference (bool toggleStatus);
  void slotClearReference ();
  void slotReferenceColor ();

  void slotSurfaceColor ();
  void slotPrimaryColor ();
  void slotSecondaryColor ();
  void slotHighlightColor ();
  void slotBackgroundColor ();
  void slotRestoreDefaults ();
  
  void about ();
  void aboutQt ();



 private:  


  void keyPressEvent (QKeyEvent *e);

  void GetLabelSpacing (void);
  void InsertGridTicks (int32_t, int32_t, char, int32_t, int32_t, int32_t, nvMap *, QColor);
  void storeZoomBounds(nvMap*, int32_t, int32_t);
  void processMultiWaves (ABE_SHARE);
  void drawMultiWaves (nvMap *, NVMAP_DEF, int32_t mapIndex);
  
  void drawMultiGrid  (CZMIL_CWF_Data wfData[], NVMAP_DEF mapDef, nvMap * map);
    
  void GetTickSpacing (int32_t, int32_t, int32_t, int32_t, int32_t &, int32_t &, int32_t);
  QPoint *loadRectangleCoords(QPoint);

  // Function that parses out the gps time from the .hof file for the waveform data write

  QString RetrieveGPSTime (int32_t index);

  // A much needed function that just returns the map type (PMT, APD, TBDD, RAMAN, or IR).
  // This helps us not to duplicate so much code.

  int32_t GetSensorMapType (nvMap *);

  uint8_t GetMapVisibilityButtonState (int32_t mapIndex);

  // variable used to store current zooming pane for zoom out purposes
  // used in rightMouse and slotContextResetZoom

  nvMap * zoomOutMap;
};

#endif
