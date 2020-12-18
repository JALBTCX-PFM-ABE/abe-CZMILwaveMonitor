
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

#ifndef VERSION

#define     VERSION     "PFM Software - CZMIL Waveform Viewer V1.28 - 04/17/18"

#endif

/*

    Version 1.0
    Gary Morris
    07/28/2011
	
    Created a template from the SHOALS Waveform Viewer
	

    Version 1.1
    Gary Morris
    08/30/2011 
	
    - implemented the drawing of the waveforms to coincide with latest CZMIL library
    - implemented NN mode


    Version 1.11
    Jan C. Depner
    12/02/2011

    Replaced .xpm files with .png files for easier HTML documentation.


    Version 1.12
    Jan C. Depner
    07/15/12

    Added support for preliminary CZMIL CPF and CWF files.


    Version 1.13
    Jan C. Depner
    10/31/12

    Now places X on interest point on the waveform that the point was derived from.
    Happy Halloween!
    Only 2 shopping days to retirement!


    Version 1.14
    Jan C. Depner (PFM Software)
    12/09/13

    Switched to using .ini file in $HOME (Linux) or $USERPROFILE (Windows) in the ABE.config directory.  Now
    the applications qsettings will not end up in unknown places like ~/.config/navo.navy.mil/blah_blah_blah on
    Linux or, in the registry (shudder) on Windows.


    Version 1.15
    Jan C. Depner (PFM Software)
    12/17/13

    Made color preferences actually work.  Removed surface return color and replaced it with text color.


    Version 1.16
    Jan C. Depner (PFM Software)
    12/19/13

    Using ip_rank = 0 to determine surface return for water returns.
    Fixed Save option.


    Version 1.17
    Jan C. Depner (PFM Software)
    12/20/13

    Split IR window into IR and T0 windows.


    Version 1.18
    Jan C. Depner (PFM Software)
    01/22/14

    - Fixed zoom!
    - Added highlight axis for selected return.
    - Made hotkeys from pfmEdit3D work.
    - Made hotkeys from pfmEdit3D work when pressed with focus in CZMILwaveMonitor.


    Version 1.19
    Jan C. Depner (PFM Software)
    01/23/14

    - Added current channel mode.


    Version 1.20
    Jan C. Depner (PFM Software)
    01/24/14

    - Added +/- options to move forward or backward in the file.


    Version 1.21
    Jan C. Depner (PFM Software)
    07/01/14

    - Replaced all of the old, borrowed icons with new, public domain icons.  Mostly from the Tango set
      but a few from flavour-extended and 32pxmania.


    Version 1.22
    Jan C. Depner (PFM Software)
    07/23/14

    - Switched from using the old NV_INT64 and NV_U_INT32 type definitions to the C99 standard stdint.h and
      inttypes.h sized data types (e.g. int64_t and uint32_t).


    Version 1.23
    Jan C. Depner (PFM Software)
    02/11/15

    - Got rid of the compile time CZMIL naming BS.  It now checks the ABE_CZMIL environment variable at run time.


    Version 1.24
    Jan C. Depner (PFM Software)
    02/16/15

    - To give better feedback to shelling programs in the case of errors I've added the program name to all
      output to stderr.


    Version 1.25
    Jan C. Depner (PFM Software)
    08/26/16

    - Now uses the same font as all other ABE GUI apps.  Font can only be changed in pfmView Preferences.


    Version 1.26
    Jan C. Depner (PFM Software)
    04/19/17

    - Properly marks ip_rank = 0 returns for waveforms that were CZMIL_OPTECH_CLASS_HYBRID processed.


    Version 1.27
    Jan C. Depner (PFM Software)
    09/26/17

    - A bunch of changes to support doing translations in the future.  There is a generic
      czmil2LAS_xx.ts file that can be run through Qt's "linguist" to translate to another language.


    Version 1.28
    Jan C. Depner (PFM Software)
    04/17/18

    - Increased time scale length of IR waveform window from 192 to 640.
    - Fixed time scale annotation for IR and T0 waveform windows.
    - Added zero time tick to all windows.

*/
