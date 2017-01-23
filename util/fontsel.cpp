/*******************************************************************************
*                                                                              *
* fontsel.c -- Nirvana Font Selector                                           *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* June 2, 1993                                                                 *
*                                                                              *
* Written by Suresh Ravoor (assisted by Mark Edel)                             *
*                                                                              *
*******************************************************************************/

#include "fontsel.h"
#include "misc.h"
#include "nedit_malloc.h"
#include "DialogF.h"

#include <string>
#include <sstream>
#include <set>
#include <vector>
#include <iostream>
#include <cmath>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/List.h>
#include <Xm/Label.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/MessageB.h>
#include <Xm/DialogS.h>

#include <Xft/Xft.h>

#define MAX_ARGS                        20
#define MAX_NUM_FONTS                   32767
#define MAX_FONT_NAME_LEN               256
#define MAX_ENTRIES_IN_LIST             5000
#define MAX_DISPLAY_SIZE                150
#define DELIM                           '-'
#define NUM_COMPONENTS_FONT_NAME        14
#define TEMP_BUF_SIZE                   256
#define DISPLAY_HEIGHT                  90

enum listSpecifier { NONE, FONT, STYLE, SIZE };

/* local data structures and types */

struct xfselControlBlkType
{
        Widget          form;           /* widget id */
        Widget          okButton;       /* widget id */
        Widget          cancelButton;   /* widget id */
        Widget          fontList;       /* widget id */
        Widget          styleList;      /* widget id */
        Widget          sizeList;       /* widget id */
        Widget          fontNameField;  /* widget id */
        Widget          sizeToggle;     /* widget id */
        Widget          propFontToggle; /* widget id */
        Widget          dispField;      /* widget id */
        std::vector<std::string> fontData;     /* font name info  */
        std::string     sel1;           /* selection from list 1 */
        std::string     sel2;           /* selection from list 2 */
        std::string     sel3;           /* selection from list 3 */
        int             showPropFonts;  /* toggle state - show prop fonts */
        bool            showSizeInPixels;/* toggle state - size in pixels  */
        std::string     fontName;       /* current font name */
        XftFont*        oldFont = nullptr;        /* font data structure for dispSample */
        XmRendition     rendition = nullptr;      /* font data structure for dispSample */
        XmRenderTable   renderTable = nullptr;    /* font data structure for dispSample */
        int     exitFlag;               /* used for program exit control */
        int     destroyedFlag;          /* used to prevent double destruction */
        Pixel   sampleFG;               /* Colors for the sample field */
        Pixel   sampleBG;
};


/* local function prototypes */

static std::string getStringComponent(const std::string& inStr, int pos, char delim = '-');
static void setupScrollLists(int dontChange, xfselControlBlkType& ctrlBlk);
static bool notPropFont(const std::string& font);
static bool styleMatch(xfselControlBlkType *ctrlBlk, const std::string& font);
static bool fontMatch(xfselControlBlkType *ctrlBlk, const std::string& font);
static std::string getFamilyPart(const std::string& font);
static std::string getStylePart(const std::string& font);
static void     propFontToggleAction(Widget widget,
                                     xfselControlBlkType *ctrlBlk,
                                     XmToggleButtonCallbackStruct *call_data);
static void     sizeToggleAction(Widget widget,
                                 xfselControlBlkType *ctrlBlk,
                                 XmToggleButtonCallbackStruct *call_data);
static void     fontAction(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmListCallbackStruct *call_data);
static void     styleAction(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmListCallbackStruct *call_data);
static void     sizeAction(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmListCallbackStruct *call_data);
static void     choiceMade(xfselControlBlkType *ctrlBlk);
static void     dispSample(xfselControlBlkType *ctrlBlk);
static void     destroyCB(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmListCallbackStruct *call_data);
static void     cancelAction(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmListCallbackStruct *call_data);
static void     okAction(Widget widget, xfselControlBlkType *ctrlBlk,
                                 XmPushButtonCallbackStruct *call_data);
static void startupFont(xfselControlBlkType *ctrlBlk, const std::string& font);
static void     setFocus(Widget w, xfselControlBlkType *ctrlBlk, XEvent *event,
                                                Boolean *continueToDispatch);
static void enableSample(xfselControlBlkType *ctrlBlk, bool turn_on, XmRenderTable* );

/*******************************************************************************
*                                                                              *
*     FontSel ()                                                               *
*                                                                              *
*                                                                              *
*            Function to put up a modal font selection dialog box. The purpose *
*            of this routine is to allow the user to interactively view sample *
*            fonts and to choose a font for current use.                       *
*                                                                              *
*     Arguments:                                                               *
*                                                                              *
*            Widget     parent          - parent widget ID                     *
*                                                                              *
*            int        showPropFont    - ONLY_FIXED : shows only fixed fonts  *
*                                                      doesn't show prop font  *
*                                                      toggle button also.     *
*                                         PREF_FIXED : can select either fixed *
*                                                      or proportional fonts;  *
*                                                      but starting option is  *
*                                                      Fixed fonts.            *
*                                         PREF_PROP  : can select either fixed *
*                                                      or proportional fonts;  *
*                                                      but starting option is  *
*                                                      proportional fonts.     *
*                                                                              *
*           char *      currFont        - ASCII string that contains the name  *
*                                         of the currently selected font.      *
*                                                                              *
*           Pixel   sampleFG, sampleBG      - Foreground/Background colors in  *
*                                               which to display the sample    *
*                                               text.                          *
*                                                                              *
*                                                                              *
*     Returns:                                                                 *
*                                                                              *
*           pointer to an ASCII character string that contains the name of     *
*           the selected font (in X format for naming fonts); it is the users  *
*           responsibility to free the space allocated to this string.         *
*                                                                              *
*     Comments:                                                                *
*                                                                              *
*           The calling function has to call the appropriate routines to set   *
*           the current font to the one represented by the returned string.    *
*                                                                              *
*******************************************************************************/

std::string FontSel(Widget parent, int showPropFonts, const std::string& currFont, Pixel sampleFG, Pixel sampleBG)
{
    Widget          propFontToggle = nullptr;
    xfselControlBlkType ctrlBlk;

    XftFontSet* fontSet = XftListFonts(nullptr, 0,
            0,
            XFT_FAMILY,
            XFT_STYLE,
            0);

    for(int i = 0; fontSet && i < fontSet->nfont; ++i) {
        char buffer[512];
        XftNameUnparse(fontSet->fonts[i], &buffer[0], sizeof(buffer));

        std::string fontName = getFamilyPart(buffer) + ":" + getStylePart(buffer);
        ctrlBlk.fontData.push_back(fontName);
    }
    XftFontSetDestroy(fontSet);

    // Create a default standard font
    ctrlBlk.oldFont = XftFontOpenName(XtDisplay(parent), DefaultScreen(parent), "Arial:style=Regular:size=14");
    if (ctrlBlk.oldFont) {
        neditxx::Args args{XmNfontType, XmFONT_IS_XFT, XmNxftFont, ctrlBlk.oldFont};
        ctrlBlk.rendition = XmRenditionCreate(parent, (char*)"renditionTag1", args.list(), args.size());
        ctrlBlk.renderTable = XmRenderTableAddRenditions(nullptr, &ctrlBlk.rendition, 1, XmMERGE_REPLACE);
    }

    ctrlBlk.sampleFG = sampleFG;
    ctrlBlk.sampleBG = sampleBG;

    Widget dialog  = neditxx::CreateDialogShell(parent, "Font Selector");

    /*  Set up window sizes for form widget */
    /*  Create form popup dialog widget */

    Widget form = neditxx::XtCreateWidget ("Font Selector", xmFormWidgetClass, dialog,
                        neditxx::Args{
                            XmNautoUnmanage, false,
                            XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL});

    /*  Create pushbutton widgets */

    Widget okButton = neditxx::XtCreateManagedWidget("OK", xmPushButtonWidgetClass, form,
            neditxx::Args {
                XmNbottomAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNbottomOffset, 4,
                XmNtopOffset, 1,
                XmNrightPosition, 45,
                XmNwidth, 110,
                XmNheight, 28,
                XmNshowAsDefault, true });

    Widget cancelButton = neditxx::XtCreateManagedWidget("Cancel", xmPushButtonWidgetClass, form,
            neditxx::Args {
                XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNtopWidget, okButton,
                XmNbottomWidget, okButton,
                XmNleftPosition, 55,
                XmNwidth, 110,
                XmNheight, 28});

    /*  create font name text widget and the corresponding label */

    Widget fontName = XtCreateManagedWidget("fontname", xmTextWidgetClass, form,
            neditxx::Args {
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNbottomWidget, okButton,
                XmNleftPosition, 1,
                XmNrightPosition, 99,
                XmNeditable, true,
                XmNeditMode, XmSINGLE_LINE_EDIT,
                XmNmaxLength, MAX_FONT_NAME_LEN});

    RemapDeleteKey(fontName);   /* kludge to handle delete and BS */

    Widget nameLabel = XtCreateManagedWidget("Font Name:", xmLabelWidgetClass, form,
            neditxx::Args {
                XmNlabelString, neditxx::XmString("Font Name:"),
                XmNmnemonic, 'N',
                XmNuserData, fontName,
                XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNleftWidget, fontName,
                XmNbottomWidget, fontName,
                XmNtopOffset, 1});

    /*  create sample display text field widget */

    Widget dispField = neditxx::XtCreateManagedWidget(" ", xmTextFieldWidgetClass, form,
            neditxx::Args {
                XmNleftAttachment, XmATTACH_POSITION,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNrightPosition, 99,
                XmNbottomWidget, nameLabel,
                XmNleftPosition, 1,
                XmNalignment, XmALIGNMENT_BEGINNING,
                XmNvalue, "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789",
                XmNforeground, sampleFG,
                XmNbackground, sampleBG});

    Widget sampleLabel = neditxx::XtCreateManagedWidget("Font Name:", xmLabelWidgetClass, form,
            neditxx::Args {
                XmNlabelString, neditxx::XmString("Sample:"),
                XmNmnemonic, 'S',
                XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNleftWidget, dispField,
                XmNbottomWidget, dispField,
                XmNtopOffset, 1});

    /*  create toggle buttons */

    Widget sizeToggle = neditxx::XtCreateManagedWidget("sizetoggle", xmToggleButtonWidgetClass, form,
            neditxx::Args {
                XmNlabelString, neditxx::XmString("Show Size in Points:"),
                XmNmnemonic, 'P',
                XmNleftAttachment, XmATTACH_POSITION,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNleftPosition, 2,
                XmNtopOffset, 1,
                XmNbottomWidget, sampleLabel});

    if (showPropFonts != ONLY_FIXED)
    {
        propFontToggle = XtCreateManagedWidget("propfonttoggle", xmToggleButtonWidgetClass, form,
                neditxx::Args {
                    XmNlabelString, neditxx::XmString("Show Proportional Width Fonts"),
                    XmNmnemonic, 'W',
                    XmNrightAttachment, XmATTACH_POSITION,
                    XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                    XmNrightPosition, 98,
                    XmNtopWidget, sizeToggle,
                    XmNbottomWidget, sizeToggle,
                    XmNtopOffset, 1});
    }

    /*  create scroll list widgets */
    /*  "Font" list */

    nameLabel = neditxx::XtCreateManagedWidget("Font:", xmLabelWidgetClass, form,
            neditxx::Args {
                XmNlabelString, neditxx::XmString("Font:"),
                XmNmnemonic, 'F',
                XmNtopOffset, 2,
                XmNtopAttachment, XmATTACH_FORM,
                XmNleftAttachment, XmATTACH_POSITION,
                XmNleftPosition, 1});

    Widget fontList = neditxx::XmCreateScrolledList(form, "fontlist",
            neditxx::Args {
                XmNvisibleItemCount, 15,
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNbottomWidget, sizeToggle,
                XmNtopWidget, nameLabel,
                XmNleftWidget, nameLabel,
                XmNrightPosition, 52});

    AddMouseWheelSupport(fontList);
    XtManageChild(fontList);
    XtVaSetValues(nameLabel, XmNuserData, fontList, 0);

    /* "Style" list */

    Widget styleList = neditxx::XmCreateScrolledList(form, "stylelist",
            neditxx::Args {
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNtopWidget, nameLabel,
                XmNleftOffset, 5,
                XmNleftWidget, XtParent(fontList),
                XmNbottomWidget, XtParent(fontList),
                XmNrightPosition, 85});

    AddMouseWheelSupport(styleList);
    XtManageChild(styleList);

    XtCreateManagedWidget("Style:", xmLabelWidgetClass, form,
            neditxx::Args {
                XmNlabelString,  neditxx::XmString("Style:"),
                XmNmnemonic, 'y',
                XmNuserData, styleList,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNbottomWidget, XtParent(styleList),
                XmNleftWidget, XtParent(styleList)});

    /*  "Size" list */

    Widget sizeList = neditxx::XmCreateScrolledList(form, "sizelist",
            neditxx::Args {
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNleftAttachment, XmATTACH_WIDGET,
                XmNrightAttachment, XmATTACH_POSITION,
                XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNtopWidget, nameLabel,
                XmNleftWidget, XtParent(styleList),
                XmNbottomWidget, XtParent(fontList),
                XmNleftOffset, 5,
                XmNrightPosition, 99});

    AddMouseWheelSupport(sizeList);
    XtManageChild(sizeList);

    XtCreateManagedWidget("Size:", xmLabelWidgetClass, form,
            neditxx::Args {
                XmNlabelString,  neditxx::XmString("Size:"),
                XmNmnemonic, 'z',
                XmNuserData, sizeList,
                XmNbottomAttachment, XmATTACH_WIDGET,
                XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
                XmNbottomWidget, XtParent(sizeList),
                XmNleftWidget, XtParent(sizeList)});

    /*  update form widgets cancel button */

    XtSetValues(form, neditxx::Args{XmNcancelButton, cancelButton});

    /*  update application's control block structure */

    ctrlBlk.form            = form;
    ctrlBlk.okButton        = okButton;
    ctrlBlk.cancelButton    = cancelButton;
    ctrlBlk.fontList        = fontList;
    ctrlBlk.styleList       = styleList;
    ctrlBlk.sizeList        = sizeList;
    ctrlBlk.fontNameField   = fontName;
    ctrlBlk.sizeToggle      = sizeToggle;
    if (showPropFonts != ONLY_FIXED)
        ctrlBlk.propFontToggle  = propFontToggle;
    ctrlBlk.dispField       = dispField;
    ctrlBlk.exitFlag        = false;
    ctrlBlk.destroyedFlag   = false;
    ctrlBlk.showPropFonts   = showPropFonts;
    ctrlBlk.showSizeInPixels = true;

    setupScrollLists(NONE, ctrlBlk);    /* update scroll lists */

    if (showPropFonts == PREF_PROP)
        XmToggleButtonSetState(propFontToggle, true, false);

    /*  Register callback functions */

    if (showPropFonts != ONLY_FIXED)
        XtAddCallback(propFontToggle, XmNvalueChangedCallback, (XtCallbackProc)propFontToggleAction, (char *)&ctrlBlk);
    XtAddCallback(sizeToggle, XmNvalueChangedCallback, (XtCallbackProc)sizeToggleAction, (char *)&ctrlBlk);
    XtAddCallback(fontList, XmNbrowseSelectionCallback, (XtCallbackProc)fontAction, (char *)&ctrlBlk);
    XtAddCallback(styleList, XmNbrowseSelectionCallback, (XtCallbackProc)styleAction, (char *)&ctrlBlk);
    XtAddCallback(sizeList, XmNbrowseSelectionCallback, (XtCallbackProc)sizeAction, (char *)&ctrlBlk);
    XtAddCallback(okButton, XmNactivateCallback, (XtCallbackProc)okAction, (char *)&ctrlBlk);
    XtAddCallback(cancelButton, XmNactivateCallback, (XtCallbackProc)cancelAction, (char *)&ctrlBlk);

    /* add event handler to setup input focus at start to scroll list */

    XtAddEventHandler(fontList, FocusChangeMask, false, (XtEventHandler)setFocus, (char *)&ctrlBlk);
    XmProcessTraversal(fontList, XmTRAVERSE_CURRENT);

    /*  setup tabgroups */

    XmAddTabGroup(fontList);
    XmAddTabGroup(styleList);
    XmAddTabGroup(sizeList);
    XmAddTabGroup(sizeToggle);
    if (showPropFonts != ONLY_FIXED)
        XmAddTabGroup(propFontToggle);
    XmAddTabGroup(fontName);
    XmAddTabGroup(okButton);
    XmAddTabGroup(cancelButton);

    /* Make sure that we don't try to access the dialog if the user
       destroyed it (possibly indirectly, by destroying the parent). */
    XtAddCallback(dialog, XmNdestroyCallback, (XtCallbackProc)destroyCB, (char *)&ctrlBlk);

    /*  Link Motif Close option to cancel action */

    AddMotifCloseCallback(dialog, (XtCallbackProc)cancelAction, &ctrlBlk);

    /*  Handle dialog mnemonics  */

    AddDialogMnemonicHandler(form, false);

    /*  Realize Widgets  */

    ManageDialogCenteredOnPointer(form);

    /* set up current font parameters */

    if (currFont.size())
        startupFont(&ctrlBlk, currFont);

    /* Make sure that we can still access the display in case the form
       gets destroyed */
    Display* theDisplay = XtDisplay(form);

    /*  enter event loop */

    while (!ctrlBlk.exitFlag && !ctrlBlk.destroyedFlag)
        XtAppProcessEvent(XtWidgetToApplicationContext(form), XtIMAll);

    if (!ctrlBlk.destroyedFlag) {
        /* Don't let the callback destroy the font name */
        XtRemoveCallback(dialog, XmNdestroyCallback, (XtCallbackProc)destroyCB, (char *)&ctrlBlk);
        XtDestroyWidget(dialog);
    }

    if (ctrlBlk.oldFont)
        XftFontClose(theDisplay, ctrlBlk.oldFont);

    if (ctrlBlk.rendition) {
        XmRenditionFree(ctrlBlk.rendition);
    }

    if (ctrlBlk.renderTable)
        XmRenderTableFree(ctrlBlk.renderTable);

    return ctrlBlk.fontName;
}

/*  gets a specific substring from a string */

static
std::string
getStringComponent(const std::string& str, int index, char delim)
{
    std::string s_index;

    if (index >= 0) {
        std::string::size_type first = 0, last = str.find(delim);

        while(index > 0 && last != std::string::npos) {
            first = last + 1;
            last = str.find(delim, last + sizeof(delim));
            --index;
        }

        if (index == 0) {
            if (last == std::string::npos) {
                last = str.size();
            }
            s_index = str.substr(first, last - first);
        }
    }
    return s_index;
}

/* parse through the fontlist data and set up the three scroll lists */

static
void setupScrollLists(int dontChange, xfselControlBlkType& ctrlBlk)
{
    std::set<std::string> family_items;
    std::set<std::string> style_items;
    std::vector<std::string> size_items;

    for (const auto& font : ctrlBlk.fontData)
    {
        if ((dontChange != FONT)
            && (styleMatch(&ctrlBlk, font))
            && ((ctrlBlk.showPropFonts == PREF_PROP) || (notPropFont(font))))
        {
            family_items.insert(getFamilyPart(font));
        }

        if ((dontChange != STYLE)
            && (fontMatch(&ctrlBlk, font))
            && ((ctrlBlk.showPropFonts == PREF_PROP) || (notPropFont(font))))
        {
            style_items.insert(getStylePart(font));
        }
    }

    if (dontChange != SIZE) {
        for (int i = 6; i < 32; ++i) {
            size_items.push_back(std::to_string(i));
        }
    }

    //  recreate all three scroll lists where necessary
    if (dontChange != FONT)
    {
        std::vector<XmString> items;
        for (const auto& family : family_items)
            items.push_back(neditxx::XmStringCreate(family));

        XmListDeleteAllItems(ctrlBlk.fontList);
        XmListAddItems(ctrlBlk.fontList, items.data(), items.size(), 1);

        if (ctrlBlk.sel1.size())
        {
            XmStringFree(items[0]);
            items[0] = neditxx::XmStringCreate(ctrlBlk.sel1);
            XmListSelectItem(ctrlBlk.fontList, items[0], false);
            XmListSetBottomItem(ctrlBlk.fontList, items[0]);
        }

        for (auto& item : items)
            XmStringFree(item);
    }

    if (dontChange != STYLE)
    {
        std::vector<XmString> items;
        for(const auto& style : style_items)
            items.push_back(neditxx::XmStringCreate(style));

        XmListDeleteAllItems(ctrlBlk.styleList);
        XmListAddItems(ctrlBlk.styleList, items.data(), items.size(), 1);

        if (ctrlBlk.sel2.size())
        {
            XmStringFree(items[0]);
            items[0] = neditxx::XmStringCreate(ctrlBlk.sel2);
            XmListSelectItem(ctrlBlk.styleList, items[0], false);
            XmListSetBottomItem(ctrlBlk.styleList, items[0]);
        }

        for (auto& item : items)
            XmStringFree(item);
    }

    if (dontChange != SIZE)
    {
        std::vector<XmString> items;
        for(const auto& size : size_items)
            items.push_back(neditxx::XmStringCreate(size));

        XmListDeleteAllItems(ctrlBlk.sizeList);
        XmListAddItems(ctrlBlk.sizeList, items.data(), items.size(), 1);

        if (ctrlBlk.sel3.size())
        {
            XmStringFree(items[0]);
            items[0] = neditxx::XmStringCreate(ctrlBlk.sel3);
            XmListSelectItem(ctrlBlk.sizeList, items[0], false);
            XmListSetBottomItem(ctrlBlk.sizeList, items[0]);
        }

        for (auto& item : items)
            XmStringFree(item);
    }
}

/*  returns true if argument is not name of a proportional font */

static
bool notPropFont(const std::string& font)
{
    std::string prop = getStringComponent(font, 11);
    return !(prop == "p" || prop == "P");
}

/*  returns true if the style portion of the font matches the currently
    selected style */

static
bool styleMatch(xfselControlBlkType *ctrlBlk, const std::string& font)
{
    return (ctrlBlk->sel2.empty()
            || ctrlBlk->sel2 == getStylePart(font));
}

/*  returns true if the font portion of the font matches the currently
    selected font */

static
bool fontMatch(xfselControlBlkType *ctrlBlk, const std::string& font)
{
    return (ctrlBlk->sel1.empty()
            || ctrlBlk->sel1 == getFamilyPart(font));
}

/*  given a font name this function returns the part used in the first 
    scroll list */

static
std::string getFamilyPart(const std::string& font)
{
    return getStringComponent(font, 0,  ':');
}

/*  given a font name this function returns the part used in the second 
    scroll list */

static
std::string getStylePart(const std::string& font)
{
    std::string stylePart = getStringComponent(font, 1, ':');

    if (stylePart.find("Regular") != std::string::npos) {
        stylePart = "style=Regular";
    } else if (stylePart.find("Bold,") != std::string::npos) {
        stylePart = "style=Bold";
    } else if (stylePart.find("Italic,") != std::string::npos) {
        stylePart = "style=Italic";
    } else if (stylePart.find("Oblique,") != std::string::npos) {
        stylePart = "style=Oblique";
    } else if (stylePart.find("Bold Oblique,") != std::string::npos) {
        stylePart = "style=Bold Oblique";
    }

    return stylePart;
}

/*  Call back functions start from here - suffix Action in the function name
    is for the callback function for the corresponding widget */

static
void propFontToggleAction(Widget widget, xfselControlBlkType *ctrlBlk, XmToggleButtonCallbackStruct *call_data)
{
    if (call_data->reason == XmCR_VALUE_CHANGED)
    {
        if (ctrlBlk->showPropFonts == PREF_FIXED)
            ctrlBlk->showPropFonts = PREF_PROP;
        else
            ctrlBlk->showPropFonts = PREF_FIXED;

        ctrlBlk->sel1 = "";
        ctrlBlk->sel2 = "";
        ctrlBlk->sel3 = "";

        setupScrollLists(NONE, *ctrlBlk);

        neditxx::XmTextSetString(ctrlBlk->fontNameField, "");
        enableSample(ctrlBlk, false, nullptr);
    }
}

static
void sizeToggleAction(Widget widget, xfselControlBlkType *ctrlBlk, XmToggleButtonCallbackStruct *call_data)
{
    if (call_data->reason == XmCR_VALUE_CHANGED)
    {
        bool makeSelection = (ctrlBlk->sel3.size());
        std::string newSize = "14";

        if (ctrlBlk->showSizeInPixels)
            ctrlBlk->showSizeInPixels = false;
        else
            ctrlBlk->showSizeInPixels = true;

        ctrlBlk->sel3 = "";
        setupScrollLists(NONE, *ctrlBlk);

        if (makeSelection)
        {
            auto str = neditxx::XmStringCreate(newSize);
            XmListSelectItem(ctrlBlk->sizeList, str, true);
            XmListSetBottomItem(ctrlBlk->sizeList, str);
            XmStringFree(str);
        }
    }
}

static
void enableSample(xfselControlBlkType *ctrlBlk, bool turn_on, XmRenderTable* renderTable)
{
    neditxx::Args args{
        XmNeditable, turn_on,
        XmNcursorPositionVisible, turn_on};

    if (turn_on) {
        args.add(XmNforeground, ctrlBlk->sampleFG);
        args.add(XmNbackground, ctrlBlk->sampleBG);
        args.add(XmNrenderTable, *renderTable);
    }

    neditxx::XtSetValues(ctrlBlk->dispField, args);

    // Make sure the sample area gets resized if the font size changes
    XtUnmanageChild(ctrlBlk->dispField);
    XtManageChild(ctrlBlk->dispField);
}

static
void fontAction(Widget widget, xfselControlBlkType *ctrlBlk, XmListCallbackStruct *call_data)
{
    std::string sel = neditxx::XmStringGetLtoR(call_data->item);

    if (ctrlBlk->sel1.empty())
    {
        ctrlBlk->sel1 = sel;
    }
    else
    {
        if (ctrlBlk->sel1 == sel)
        {   // Unselecting current selection
            ctrlBlk->sel1 = "";
            XmListDeselectItem(widget, call_data->item);
        }
        else
        {
            ctrlBlk->sel1 = sel;
        }
    }

    setupScrollLists(FONT, *ctrlBlk);
    if ((ctrlBlk->sel1.size()) && (ctrlBlk->sel2.size()) && (ctrlBlk->sel3.size()))
        choiceMade(ctrlBlk);
    else
    {
        enableSample(ctrlBlk, false, nullptr);
        neditxx::XmTextSetString(ctrlBlk->fontNameField, "");
    }
}

static
void styleAction(Widget widget, xfselControlBlkType *ctrlBlk, XmListCallbackStruct *call_data)
{
    std::string sel = neditxx::XmStringGetLtoR(call_data->item);

    if (ctrlBlk->sel2.empty())
    {
        ctrlBlk->sel2 = sel;
    }
    else
    {
        if (ctrlBlk->sel2 == sel)
        {   // unselecting current selection
            ctrlBlk->sel2 = "";
            XmListDeselectItem(widget, call_data->item);
        }
        else
        {
            ctrlBlk->sel2 = sel;
        }
    }

    setupScrollLists(STYLE, *ctrlBlk);
    if ((ctrlBlk->sel1.size()) && (ctrlBlk->sel2.size()) && (ctrlBlk->sel3.size()))
        choiceMade(ctrlBlk);
    else
    {
        enableSample(ctrlBlk, false, nullptr);
        XmTextSetString(ctrlBlk->fontNameField, (char*)"");
    }
}

static
void sizeAction(Widget widget, xfselControlBlkType *ctrlBlk, XmListCallbackStruct *call_data)
{
    std::string sel = neditxx::XmStringGetLtoR(call_data->item);

    if (ctrlBlk->sel3.empty())
    {
        ctrlBlk->sel3 = sel;
    }
    else
    {
        if (ctrlBlk->sel3 == sel)
        {   // unselecting current selection
            ctrlBlk->sel3 = "";
            XmListDeselectItem(widget, call_data->item);
        }
        else
        {
            ctrlBlk->sel3 = sel;
        }
    }

    setupScrollLists(SIZE, *ctrlBlk);
    if ((ctrlBlk->sel1.size()) && (ctrlBlk->sel2.size()) && (ctrlBlk->sel3.size()))
        choiceMade(ctrlBlk);
    else
    {
        enableSample(ctrlBlk, false, nullptr);
        neditxx::XmTextSetString(ctrlBlk->fontNameField, "");
    }
}

/*  function called when all three choices have been made; sets up font
    name and displays sample font */

static
void choiceMade(xfselControlBlkType *ctrlBlk)
{
    ctrlBlk->fontName = "";

    for (const auto& font : ctrlBlk->fontData)
    {
        if (fontMatch(ctrlBlk, font) && styleMatch(ctrlBlk, font))
        {
            ctrlBlk->fontName = font;
            break;
        }
    }

    if (!ctrlBlk->fontName.empty())
    {
        ctrlBlk->fontName += ":size=" + ctrlBlk->sel3;
        neditxx::XmTextSetString(ctrlBlk->fontNameField, ctrlBlk->fontName);
        dispSample(ctrlBlk);
    }
    else
    {
        DialogF (DF_ERR, ctrlBlk->form, 1, "Font Specification", "Invalid Font Specification", "OK");
    }
}

/*  loads selected font and displays sample text in that font */

static
void dispSample(xfselControlBlkType *ctrlBlk)
{
    Display* display = XtDisplay(ctrlBlk->form);
    XftFont* font = XftFontOpenName(display, DefaultScreen(display), ctrlBlk->fontName.c_str());

    neditxx::Args args{XmNfontType, XmFONT_IS_XFT, XmNxftFont, font};

    if (ctrlBlk->rendition) {
        XmRenditionFree(ctrlBlk->rendition);
    }

    ctrlBlk->rendition = XmRenditionCreate(ctrlBlk->form, (char*)"renditionTag1", args.list(), args.size());
    ctrlBlk->renderTable = XmRenderTableAddRenditions(ctrlBlk->renderTable, &ctrlBlk->rendition, 1, XmMERGE_REPLACE);

    enableSample(ctrlBlk, true, &ctrlBlk->renderTable);

    if (ctrlBlk->oldFont)
        XftFontClose(display, ctrlBlk->oldFont);

    ctrlBlk->oldFont    = font;
}

static
void destroyCB(Widget widget, xfselControlBlkType *ctrlBlk, XmListCallbackStruct *call_data)
{
    /* Prevent double destruction of the font selection dialog */
    ctrlBlk->destroyedFlag = true;
    cancelAction(widget, ctrlBlk, call_data);
}

static
void cancelAction(Widget widget, xfselControlBlkType *ctrlBlk, XmListCallbackStruct *call_data)
{
    ctrlBlk->fontName = "";
    ctrlBlk->exitFlag = true;
}

static
void okAction(Widget widget, xfselControlBlkType *ctrlBlk, XmPushButtonCallbackStruct *call_data)
{
    std::string fontPattern = neditxx::XmTextGetString(ctrlBlk->fontNameField);

    int i;
    char** fontName = XListFonts(XtDisplay(ctrlBlk->form), fontPattern.c_str(), 1, &i);

    if ((fontName == nullptr) || (i == 0))
    {
        DialogF(DF_ERR, ctrlBlk->okButton, 1, "Font Specification", "Invalid Font Specification", "OK");
    }
    else
    {
        ctrlBlk->fontName = fontName[0];
        ctrlBlk->exitFlag = true;
    }

    XFreeFontNames(fontName);
}

/*  if current font is passed as an argument then this function is
    invoked and sets up initial entries */

static
void startupFont(xfselControlBlkType *ctrlBlk, const std::string& fontName)
{
    Display* display = XtDisplay(ctrlBlk->form);

    XftFont* font = XftFontOpenName(display, DefaultScreen(display), fontName.c_str());
    if (!font) {
        std::cerr << "invalid startup font name" << std::endl;
        return;
    }
    XftFontClose(display,  font);

    ctrlBlk->fontName = fontName;
    auto fontPart = getFamilyPart(fontName);
    auto str = neditxx::XmStringCreate(fontPart);
    XmListSetBottomItem(ctrlBlk->fontList, str);
    XmListSelectItem(ctrlBlk->fontList, str, true);
    XmListSelectItem(ctrlBlk->fontList, str, true);
    XmStringFree(str);

    dispSample(ctrlBlk);
    neditxx::XmTextSetString(ctrlBlk->fontNameField, ctrlBlk->fontName);
}

/*  hacked code to move initial input focus to first scroll list and at the 
    same time have the OK button as the default button */

static
void setFocus(Widget w, xfselControlBlkType *ctrlBlk, XEvent *event, Boolean *continueToDispatch)
{
    *continueToDispatch = true;

    XtVaSetValues(ctrlBlk->form, XmNdefaultButton, ctrlBlk->okButton, 0);
}
