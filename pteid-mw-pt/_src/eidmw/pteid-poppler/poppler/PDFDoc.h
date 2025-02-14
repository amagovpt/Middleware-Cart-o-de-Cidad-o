/*-****************************************************************************

 * Copyright (C) 2012, 2014, 2017-2024 André Guerreiro - <aguerreiro1985@gmail.com>
 *
 * Licensed under the EUPL V.1.2

****************************************************************************-*/

//========================================================================
//
// PDFDoc.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2005, 2006, 2008 Brad Hards <bradh@frogmouth.net>
// Copyright (C) 2005, 2009 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2008 Julien Rebetez <julienr@svn.gnome.org>
// Copyright (C) 2008 Pino Toscano <pino@kde.org>
// Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
// Copyright (C) 2009 Eric Toombs <ewtoombs@uwaterloo.ca>
// Copyright (C) 2009 Kovid Goyal <kovid@kovidgoyal.net>
// Copyright (C) 2010 Hib Eris <hib@hiberis.nl>
// Copyright (C) 2010 Srinivas Adicherla <srinivas.adicherla@geodesic.com>
// Copyright (C) 2011 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2012 Fabio D'Urso <fabiodurso@hotmail.it>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#ifndef PDFDOC_H
#define PDFDOC_H

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include <stdio.h>
#include <unordered_set>
#include <string>
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "Annot.h"
#include "poppler/poppler-config.h"
#include "OptionalContent.h"

class GooString;
class BaseStream;
class OutputDev;
class Links;
class LinkAction;
class LinkDest;
class Outline;
class Linearization;
class SecurityHandler;
class Hints;

enum PDFWriteMode {
  writeStandard,
  writeForceRewrite,
  writeForceIncremental
};


class POPPLER_API ValidationDataElement
{
public:
    enum ValidationDataType
    {
        OCSP,
        CRL,
        CERT
    };

    ValidationDataElement(unsigned char *data, size_t dataSize, ValidationDataType type) {
        this->data = new unsigned char[dataSize];
        memcpy((char*)this->data, (char*)data, dataSize);
        this->dataSize = dataSize;
        this->type = type;
    }

    ValidationDataElement(unsigned char *data, size_t dataSize, ValidationDataType type, std::unordered_set<std::string> &vriHashKeys) 
        : ValidationDataElement(data, dataSize, type) {

        for (auto const& key : vriHashKeys)
            addVriKey(key.c_str());
    }

    ValidationDataElement(const ValidationDataElement &vde)
    {
        this->data = new unsigned char[vde.dataSize];
        memcpy((char*)this->data, (char*)vde.data, vde.dataSize);
        this->dataSize = vde.dataSize;
        this->type = vde.type;

        for (auto const& key : vde.vriHashKeys)
            addVriKey(key.c_str());
    }

    ~ValidationDataElement()
    {
        delete [] this->data;
    }

    unsigned char * getData() {
        return data;
    }

    size_t getSize() {
        return dataSize;
    }

    ValidationDataType getType() {
        return type;
    }

    void addVriKey(const char *key) {
        vriHashKeys.insert(key);
    }

    std::unordered_set<std::string> &getVriHashKeys() {
        return vriHashKeys;
    }

private:
    unsigned char *data;
    size_t dataSize;
    ValidationDataType type;
    std::unordered_set<std::string> vriHashKeys;
};

//------------------------------------------------------------------------
// PDFDoc
//------------------------------------------------------------------------

class PDFDoc {
public:

POPPLER_API PDFDoc(GooString *fileNameA, GooString *ownerPassword = NULL,
	 GooString *userPassword = NULL, void *guiDataA = NULL);

#ifdef _WIN32
  POPPLER_API PDFDoc(wchar_t *fileNameA, int fileNameLen, GooString *ownerPassword = NULL,
	 GooString *userPassword = NULL, void *guiDataA = NULL);
#endif

  PDFDoc(BaseStream *strA, GooString *ownerPassword = NULL,
	 GooString *userPassword = NULL, void *guiDataA = NULL);
  POPPLER_API ~PDFDoc();

  static PDFDoc *ErrorPDFDoc(int errorCode, GooString *fileNameA = NULL);

  // Was PDF document successfully opened?
  POPPLER_API GBool isOk();

  // Get the error code (if isOk() returns false).
  int getErrorCode() { return errCode; }

  // Get the error code returned by fopen() (if getErrorCode() == 
  // errOpenFile).
  int getFopenErrno() { return fopenErrno; }

  // Get file name.
  GooString *getFileName() { return fileName; }
#ifdef _WIN32
  wchar_t *getFileNameU() { return fileNameU; }
#endif

  // Get the linearization table.
  Linearization *getLinearization();

  // Get the xref table.
  XRef *getXRef() { return xref; }

  // Get catalog.
  Catalog *getCatalog() { return catalog; }

  // Get optional content configuration
  OCGs *getOptContentConfig() { return catalog->getOptContentConfig(); }

  // Get base stream.
  BaseStream *getBaseStream() { return str; }

  // Get page parameters.
  double getPageMediaWidth(int page)
    { return getPage(page) ? getPage(page)->getMediaWidth() : 0.0 ; }
  double getPageMediaHeight(int page)
    { return getPage(page) ? getPage(page)->getMediaHeight() : 0.0 ; }
  double getPageCropWidth(int page)
    { return getPage(page) ? getPage(page)->getCropWidth() : 0.0 ; }
  double getPageCropHeight(int page)
    { return getPage(page) ? getPage(page)->getCropHeight() : 0.0 ; }
  int getPageRotate(int page)
    { return getPage(page) ? getPage(page)->getRotate() : 0 ; }

  // Get number of pages.
  POPPLER_API int getNumPages();

  // Return the contents of the metadata stream, or NULL if there is
  // no metadata.
  GooString *readMetadata() { return catalog->readMetadata(); }

  // Return the structure tree root object.
  Object *getStructTreeRoot() { return catalog->getStructTreeRoot(); }

  // Get page.
  POPPLER_API Page *getPage(int page);

  // Find a page, given its object ID.  Returns page number, or 0 if
  // not found.
  int findPage(int num, int gen) { return catalog->findPage(num, gen); }

  // Returns the links for the current page, transferring ownership to
  // the caller.
  Links *getLinks(int page);

  // Find a named destination.  Returns the link destination, or
  // NULL if <name> is not a destination.
  LinkDest *findDest(GooString *name)
    { return catalog->findDest(name); }


#ifndef DISABLE_OUTLINE
  // Return the outline object.
  Outline *getOutline();
#endif

  // Is the file encrypted?
  POPPLER_API GBool isEncrypted() { return xref->isEncrypted(); }

  /* pteid-mw - New functions that are absent from upstream Poppler */

  // Whether the file contains a form of type XFA (XML Forms Architecture) - only supported by Adobe products ?
  POPPLER_API GBool containsXfaForm();

  // Is the file signed?
  POPPLER_API GBool isSigned();
  POPPLER_API unsigned long getSigByteArray(unsigned char **byte_array);
  POPPLER_API GBool isReaderEnabled();
  /*Returns set of indexes of the signatures until (and including) the last timestamp signature. 
  The indexes are relative to the last signature: 0 is the last, 1 is the previous one, ... */
  POPPLER_API std::unordered_set<int> getSignaturesIndexesUntilLastTimestamp();
  POPPLER_API int getSignatureContents(unsigned char **, int sigIdx = 0);

  POPPLER_API Object *getByteRange();

  POPPLER_API void prepareSignature(PDFRectangle *rect, const char * name, const char *civil_number, const char *location,
		  const char *reason, int page, int sector, bool isPTLanguage, 
      bool isCCSignature, bool showDate, bool small_signature);
  POPPLER_API void addCustomSignatureImage(unsigned char *image_data, unsigned long image_length);
  POPPLER_API void addSCAPAttributes(const char *attributeSupplier, const char *attributeName);
  
  POPPLER_API char* getOccupiedSectors(int page);

  POPPLER_API void closeSignature(const char *signature_contents);

  // LTV related methods
  POPPLER_API void addDSS(std::vector<ValidationDataElement *> validationData);
  POPPLER_API void getCertsInDSS(std::vector<ValidationDataElement *> *validationData);
  POPPLER_API void prepareTimestamp();
  //POPPLER_API void closeLtv(const char *signature_contents);

  /* End of PTEID Changes */

  // Check various permissions.
  GBool okToPrint(GBool ignoreOwnerPW = gFalse)
    { return xref->okToPrint(ignoreOwnerPW); }
  GBool okToPrintHighRes(GBool ignoreOwnerPW = gFalse)
    { return xref->okToPrintHighRes(ignoreOwnerPW); }
  GBool okToChange(GBool ignoreOwnerPW = gFalse)
    { return xref->okToChange(ignoreOwnerPW); }
  GBool okToCopy(GBool ignoreOwnerPW = gFalse)
    { return xref->okToCopy(ignoreOwnerPW); }
  GBool okToAddNotes(GBool ignoreOwnerPW = gFalse)
    { return xref->okToAddNotes(ignoreOwnerPW); }
  GBool okToFillForm(GBool ignoreOwnerPW = gFalse)
    { return xref->okToFillForm(ignoreOwnerPW); }
  GBool okToAccessibility(GBool ignoreOwnerPW = gFalse)
    { return xref->okToAccessibility(ignoreOwnerPW); }
  GBool okToAssemble(GBool ignoreOwnerPW = gFalse)
    { return xref->okToAssemble(ignoreOwnerPW); }


  // Is this document linearized?
  GBool isLinearized();

  // Return the document's Info dictionary (if any).
  Object *getDocInfo(Object *obj) { return xref->getDocInfo(obj); }
  Object *getDocInfoNF(Object *obj) { return xref->getDocInfoNF(obj); }

  // Return the PDF version specified by the file.
  int getPDFMajorVersion() { return pdfMajorVersion; }
  int getPDFMinorVersion() { return pdfMinorVersion; }

  //Return the PDF ID in the trailer dictionary (if any).
  GBool getID(GooString *permanent_id, GooString *update_id);

  // Save one page with another name.
  POPPLER_API int savePageAs(GooString *name, int pageNo);
  // Save this file with another name.
  POPPLER_API int saveAs(GooString *name, PDFWriteMode mode=writeStandard);
#ifdef WIN32
  POPPLER_API int saveAs(wchar_t *name, PDFWriteMode mode=writeStandard);
#endif
  // Save this file in the given output stream.
  POPPLER_API int saveAs(OutStream *outStr, PDFWriteMode mode=writeStandard);
  // Save this file with another name without saving changes
  POPPLER_API int saveWithoutChangesAs(GooString *name);
  // Save this file in the given output stream without saving changes
  POPPLER_API int saveWithoutChangesAs(OutStream *outStr);

  // Return a pointer to the GUI (XPDFCore or WinPDFCore object).
  void *getGUIData() { return guiData; }

  // rewrite pageDict with MediaBox, CropBox and new page CTM
  void replacePageDict(int pageNo, int rotate, PDFRectangle *mediaBox, PDFRectangle *cropBox, Object *pageCTM);
  void markPageObjects(Dict *pageDict, XRef *xRef, XRef *countRef, Guint numOffset);
  // write all objects used by pageDict to outStr
  Guint writePageObjects(OutStream *outStr, XRef *xRef, Guint numOffset);
  static Guint writeObject (Object *obj, Ref *ref, OutStream* outStr, XRef *xref, Guint numOffset);
  static void writeHeader(OutStream *outStr, int major, int minor);

  // Ownership goes to the caller
  static Dict *createTrailerDict (int uxrefSize, GBool incrUpdate, Guint startxRef,
                                  Ref *root, XRef *xRef, const char *fileName, Guint fileSize, GBool sig_mode=false);
  static void writeXRefTableTrailer (Dict *trailerDict, XRef *uxref, GBool writeAllEntries,
                                     Guint uxrefOffset, OutStream* outStr, XRef *xRef);
  static void writeXRefStreamTrailer (Dict *trailerDict, XRef *uxref, Ref *uxrefStreamRef,
                                      Guint uxrefOffset, OutStream* outStr, XRef *xRef);

private:
  // insert referenced objects in XRef
  void markDictionnary (Dict* dict, XRef *xRef, XRef *countRef, Guint numOffset);
  void markObject (Object *obj, XRef *xRef, XRef *countRef, Guint numOffset);
  static void writeDictionnary (Dict* dict, OutStream* outStr, XRef *xRef, Guint numOffset);

  // Add object to current file stream and return the offset of the beginning of the object
  Guint writeObject (Object *obj, Ref *ref, OutStream* outStr)
  { return writeObject(obj, ref, outStr, getXRef(), 0); }
  void writeDictionnary (Dict* dict, OutStream* outStr)
  { writeDictionnary(dict, outStr, getXRef(), 0); }
  static void writeStream (Stream* str, OutStream* outStr);
  static void writeRawStream (Stream* str, OutStream* outStr);
  void writeXRefTableTrailer (Guint uxrefOffset, XRef *uxref, GBool writeAllEntries,
                              int uxrefSize, OutStream* outStr, GBool incrUpdate);
  static void writeString (GooString* s, OutStream* outStr);
  void saveIncrementalUpdate (OutStream* outStr);
  void saveCompleteRewrite (OutStream* outStr);

  Page *parsePage(int page);
  Ref getPageRef(int page);

  // Get hints.
  Hints *getHints();

  void cleanSignatureDicts();

  PDFDoc();
  void init();
  GBool setup(GooString *ownerPassword, GooString *userPassword);
  GBool checkFooter();
  void checkHeader();
  GBool checkEncryption(GooString *ownerPassword, GooString *userPassword);
  // Get the offset of the start xref table.
  Guint getStartXRef();
  // Get the offset of the entries in the main XRef table of a
  // linearized document (0 for non linearized documents).
  Guint getMainXRefEntriesOffset();
  Guint strToUnsigned(char *s);

  GooString *fileName;
#ifdef _WIN32
  wchar_t *fileNameU;
#endif
  FILE *file;
  unsigned long m_sig_offset;

  unsigned char * m_image_data_jpeg;
  unsigned long m_image_length;
  const char * m_attribute_supplier;
  const char * m_attribute_name;

  //Insert a

  GBool signature_mode;
  BaseStream *str;
  void *guiData;
  int pdfMajorVersion;
  int pdfMinorVersion;
  unsigned long fileSize;
  Linearization *linearization;
  XRef *xref;
  SecurityHandler *secHdlr;
  Catalog *catalog;
  Hints *hints;
#ifndef DISABLE_OUTLINE
  Outline *outline;
#endif
  Page **pageCache;

  GBool ok;
  int errCode;
  //If there is an error opening the PDF file with fopen() in the constructor, 
  //then the POSIX errno will be here.
  int fopenErrno;

  Guint startXRefPos;		// offset of last xref table
};

#endif
