/*
 * Copyright (C) 2008-2009 SVOX AG, Baslerstr. 30, 8048 Zuerich, Switzerland
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <libexpat/expat.h>

#ifndef _SVOX_SSML_PARSER_H_
#define _SVOX_SSML_PARSER_H_

/**
 * SvoxSsmlParser
 * Parses SSML 1.0 XML documents and convertes it to Pico compatible text input
 */
class SvoxSsmlParser
{
 public: /* construction code */

  /**
     Constructor
     Creates Expat parser and allocates initial text storage
  */
  SvoxSsmlParser();

  /**
     Destructor
     Deallocates all resources
  */
  ~SvoxSsmlParser();

  /**
     initSuccessful
     Verifies that construction was successful
     return 1 if successful, 0 otherwise
  */
  int initSuccessful();

 public: /* public members */

  /**
     parseDocument
     Parses SSML 1.0 document passed in as argument
     @ssmldoc - SSML document, partial document input is supported
     @isFinal - indicates whether input is a complete or partial document, 1 indicates complete document, 0 indicates partial
     return Expat status code
  */
  int parseDocument(const char* ssmldoc, int isFinal);

  /**
     getParsedDocument
     Returns string containing parse result. This can be passed on to Pico for synthesis
     return parsed string, NULL if error occurred
  */
  char* getParsedDocument();

  /**
     getParsedDocumentLanguage
     Returns language string specified in xml:lang attribute of the <speak> tag
     return language code of SSML document, NULL if not set
  */
  char* getParsedDocumentLanguage();

 private: /* static callback functions */

  /**
     starttagHandler
     Static callback function for Expat start-tag events, internal use only
  */
  static void starttagHandler(void* data, const XML_Char* element, const XML_Char** attributes);

  /**
     endtagHandler
     Static callback function for Expat end-tag events, internal use only
  */
  static void endtagHandler(void* data, const XML_Char* element);

  /**
     textHandler
     Static callback function for Expat text events, internal use only
  */
  static void textHandler(void* data, const XML_Char* text, int length);

 private: /* element handlers */

  /**
     startElement
     Handles start of element, called by starttagHandler.
  */
  void startElement(const XML_Char* element, const XML_Char** attributes);

  /**
     endElement
     Handles end of element, called by endtagHandler.
  */
  void endElement(const XML_Char* element);

  /**
     textElement
     Handles text element, called by textHandler.
  */
  void textElement(const XML_Char* text, int length);

  /* helper functions */

  /**
     convertToSvoxPitch
     Convertes SSML prosody tag pitch values to SVOX Pico pitch values.
  */
  char* convertToSvoxPitch(const char* value);

  /**
     convertToSvoxRate
     Convertes SSML prosody tag rate values to SVOX Pico speed values.
  */
  char* convertToSvoxRate(const char* value);

  /**
     convertToSvoxVolume
     Convertes SSML prosody tag volume values to SVOX Pico volume values.
  */
  char* convertToSvoxVolume(const char* value);

  /**
     convertBreakStrengthToTime
     Convertes SSML break tag strength attribute values to SVOX Pico break time values.
  */
  char* convertBreakStrengthToTime(const char* value);

  /**
     growDataSize
     Increases size of internal text field.
  */
  int growDataSize(int sizeToGrow);

 private: /* data members*/

  char* m_data;          /* internal text field, holds parsed text */
  int m_datasize;        /* size of internal text field */
  XML_Parser mParser;    /* Expat XML parser pointer */
  int m_isInBreak;       /* indicator for handling break tag parsing */
  char* m_appendix;      /* holds Pico pitch, speed and volume close tags for prosody tag parsing */
  char* m_docLanguage;   /* language set in speak tag of SSML document */
};

#endif // _SVOX_SSML_PARSER_H_
